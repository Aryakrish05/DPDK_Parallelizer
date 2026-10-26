#ifndef DSLMAPS_HPP
#define DSLMAPS_HPP

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <type_traits>
#include "LockManagers.hpp"

//in the future, GLOBAL, PER_KEY and NONE
//refs for better maps in the future - but none of them allow serializability
//https://github.com/openjdk/jdk/blob/master/src/java.base/share/classes/java/util/concurrent/ConcurrentHashMap.java
//https://github.com/torvalds/linux/blob/v6.6/kernel/bpf/hashtab.c

enum class LockKind{
    AUTO
};

template<typename KeyType, typename ValueType, size_t size, typename Lock, LockKind kind/*ignore*/,typename Manager,size_t max_value_size>
class ArrayMap{
    static_assert(std::is_unsigned_v<KeyType>,"something wrong with the codegen, array keys must be uintN");
public:
    ArrayMap(const ValueType& default_value):default_(default_value){init_values();}
    //call default constructor, as these are structs, it will be 0 initialized
    ArrayMap():default_(){init_values();}
    Lock* lock_for(const KeyType& key){
        if(key>=size){
            return nullptr;
        }
        return &entries_[key].lock;
    }
    //copies by value - this guarantees that the only writes that happen are at map.put()
    bool get(const KeyType& key, ValueType& value){
        if(key>=size){
            value = default_;
            return false;
        }
        value = entries_[key].value;
        return true;
    }

    bool put(const KeyType& key, const ValueType& value, Manager& mgr){
        if(key>=size){
            return false;
        }
        mgr.addLogRecord(get_log_record(&entries_[key],Operation::UPDATE,entries_[key].value,value));
        entries_[key].value = value;
        return true;
    }

    bool erase(const KeyType& key, Manager& mgr){
        if(key>=size){
            return false;
        }
        mgr.addLogRecord(get_log_record(&entries_[key],Operation::ERASE,entries_[key].value,default_));
        entries_[key].value = default_;
        return true;
    }

    void undo(LogRecord<max_value_size>& record){
        Entry* entry = static_cast<Entry*>(record.entry_ptr);
        std::memcpy(&entry->value,&record.prev_value,sizeof(ValueType));
    }

    void commit(LogRecord<max_value_size>& record){
        //not really needed
        return;
    }

    void dump(LogRecord<max_value_size>& record,std::ostream& out){

    }
    
private:
    struct Entry{
        Lock lock;
        ValueType value;
    };
    void init_values(){
        for(size_t i=0;i<size;i++){
            entries_[i].value = default_;
        }
    }
    LogRecord<max_value_size> get_log_record(Entry* entry_ptr,Operation op,const ValueType& prev_value,const ValueType& new_value){
        return make_log_record<ArrayMap,max_value_size>(this,entry_ptr,op,prev_value,new_value);
    }
    std::array<Entry,size> entries_;
    ValueType default_;
};

template<typename Node>
class FreeList{
public:
    void insert(Node* node){
        std::lock_guard<std::mutex>lk_guard(lock_);
        node->next = head_;
        head_ = node;
    }

    //returns nullptr if empty
    Node* remove(){
        std::lock_guard<std::mutex>lk_guard(lock_);
        if(head_==nullptr){
            return nullptr;
        }
        Node* node = head_;
        head_ = head_->next;
        return node;
    }
private:
    std::mutex lock_;
    Node* head_ = nullptr;
};

//assumes that the lock is held for every function
template<typename Node, typename Lock>
class Bucket{
public:
    Lock* get_lock(){
        return &lock_;
    }

    void insert(Node* node){
        node->prev = nullptr;
        node->next = head_;
        if(head_!=nullptr){
            head_->prev = node;
        }
        head_ = node;
    }

    void remove(Node* node){
        if(head_==node){
            assert(node->prev==nullptr);
            head_ = node->next;
        }
        else{
            node->prev->next = node->next;
        }
        if(node->next!=nullptr){
            node->next->prev = node->prev;
        }
    }

    //returns the first node satisfying match, nullptr if none
    Node* find(const std::function<bool(Node*)>& match){
        Node* ptr = head_;
        while(ptr!=nullptr){
            if(match(ptr)){
                return ptr;
            }
            ptr = ptr->next;
        }
        return nullptr;
    }
private:
    Lock lock_;
    Node* head_ = nullptr;
};

template<typename KeyType, typename ValueType, size_t size, typename Lock, LockKind kind/*ignore*/,typename Manager,size_t max_value_size>
class HashMap{
    //ensure you pad the key values appropriately
    static_assert(std::has_unique_object_representations_v<KeyType>,"key type has padding bytes");
    static_assert(size>0);
public:
    HashMap():default_(){init_map();}
    HashMap(const ValueType& default_value):default_(default_value){init_map();}

    //returns a lock regardless of whether the key is present or not...
    Lock* lock_for(const KeyType& key){
        return buckets_[get_bucket_number(key)].get_lock();
    }

    //assumes that the bucket lock is held
    bool get(const KeyType& key, ValueType& value){
        Entry* entry = find_entry(buckets_[get_bucket_number(key)],key);
        if(entry==nullptr){
            value = default_;
            return false;
        }
        value = entry->value;
        return true;
    }

    //assumes that the bucket lock is held
    bool put(const KeyType& key, const ValueType& value, Manager& mgr){
        EntryBucket& bucket = buckets_[get_bucket_number(key)];
        Entry* entry = find_entry(bucket,key);
        if(entry!=nullptr){
            mgr.addLogRecord(get_log_record(entry,Operation::UPDATE,entry->value,value));
            entry->value = value;
            return true;
        }

        Entry* new_entry = freelist_.remove();
        if(new_entry==nullptr){
            return false;
        }

        mgr.addLogRecord(get_log_record(new_entry,Operation::INSERT,default_,value));
        new_entry->key = key;
        new_entry->value = value;
        bucket.insert(new_entry);
        return true;
    }

    //assumes that the bucket lock is held
    bool erase(const KeyType& key, Manager& mgr){
        EntryBucket& bucket = buckets_[get_bucket_number(key)];
        Entry* entry = find_entry(bucket,key);
        if(entry==nullptr){
            return false;
        }
        mgr.addLogRecord(get_log_record(entry,Operation::ERASE,entry->value,default_));
        bucket.remove(entry);
        //push back to freelist while committing
        return true;
    }

    void undo(LogRecord<max_value_size>& record){
        Entry* entry = static_cast<Entry*>(record.entry_ptr);
        if(record.op==Operation::UPDATE){
            std::memcpy(&entry->value,&record.prev_value,sizeof(ValueType));
        }
        else if(record.op==Operation::ERASE){
            buckets_[get_bucket_number(entry->key)].insert(entry);
        }
        else if(record.op==Operation::INSERT){
            buckets_[get_bucket_number(entry->key)].remove(entry);
            freelist_.insert(entry);
        }
    }

    void commit(LogRecord<max_value_size>& record){
        //only needed for erase - update and insert are fine only
        if(record.op==Operation::ERASE){
            freelist_.insert(static_cast<Entry*>(record.entry_ptr));
        }
    }

    void dump(LogRecord<max_value_size>& record,std::ostream& out){

    }

private:
    static constexpr size_t bucket_size = 64;

    struct Entry{
        KeyType key;
        ValueType value;
        Entry* next;
        Entry* prev;
    };

    using EntryBucket = Bucket<Entry,Lock>;

    //FNV-1a, 64-bit (draft-eastlake-fnv)
    static uint64_t hash(const KeyType& key){
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&key);
        uint64_t h = 0xcbf29ce484222325ULL;
        size_t len = sizeof(KeyType);
        while(len--){
            h ^= *p++;
            h *= 0x100000001b3ULL;
        }
        return h;
    }

    static size_t get_bucket_number(const KeyType& key){
        return hash(key)%bucket_size;
    }

    Entry* find_entry(EntryBucket& bucket, const KeyType& key){
        return bucket.find([&key](Entry* entry){
            return std::memcmp(&entry->key,&key,sizeof(KeyType))==0;
        });
    }

    void init_map(){
        for(size_t i=size;i-->0;){
            freelist_.insert(&mempool_[i]);
        }
    }

    LogRecord<max_value_size> get_log_record(Entry* entry_ptr,Operation op,const ValueType& prev_value,const ValueType& new_value){
        return make_log_record<HashMap,max_value_size>(this,entry_ptr,op,prev_value,new_value);
    }

    std::array<Entry,size> mempool_;
    std::array<EntryBucket,bucket_size> buckets_;
    ValueType default_;
    FreeList<Entry> freelist_;
};

#endif