#ifndef DSLMAPS_HPP
#define DSLMAPS_HPP

#include<array>
#include<bit>
#include<cassert>
#include<cstdint>
#include<cstring>
#include<mutex>
#include<type_traits>
#include<vector>
#include<pair>

//in the future, GLOBAL, PER_KEY and NONE
//refs for better maps in the future - but none of them allow serializability
//https://github.com/openjdk/jdk/blob/master/src/java.base/share/classes/java/util/concurrent/ConcurrentHashMap.java
//https://github.com/torvalds/linux/blob/v6.6/kernel/bpf/hashtab.c

enum class LockKind{
    AUTO
};

enum class StrategyKind{
    SNAPSHOT,
    TWOPHASE
};

template<typename KeyType, typename ValueType, uint32_t size, typename Lock, LockKind kind/*ignore*/>
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
    //copies by value - allows us to mark dirty properly for 2PL
    bool get(const KeyType& key, ValueType& value){
        if(key>=size){
            value = default_;
            return false;
        }
        value = entries_[key].value;
        return true;
    }

    bool put(const KeyType& key, const ValueType& value){
        if(key>=size){
            return false;
        }
        entries_[key].value = value;
        return true;
    }

    bool erase(const KeyType& key){
        if(key>=size){
            return false;
        }
        entries_[key].value = default_;
        return true;
    }
private:
    struct Entry{
        Lock lock;
        ValueType value;
    };
    void init_values(){
        for(uint32_t i=0;i<size;i++){
            entries_[i].value = default_;
        }
    }
    std::array<Entry,size> entries_;
    ValueType default_;
};

template<typename KeyType, typename ValueType, uint32_t size, typename Lock, LockKind kind/*ignore*/>
class HashMap{
    //ensure you pad the key values appropriately
    static_assert(std::has_unique_object_representations_v<KeyType>,"key type has padding bytes");
    static_assert(size>0);
public:
    HashMap():default_(){init_map();}
    HashMap(const ValueType& default_value):default_(default_value){init_map();}

    //returns a lock regardless of whether the key is present or not...
    Lock* lock_for(const KeyType& key){
        return &buckets_[get_bucket_number(key)].lock;
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
    bool put(const KeyType& key, const ValueType& value){
        Bucket& bucket = buckets_[get_bucket_number(key)];
        Entry* entry = find_entry(bucket,key);
        if(entry!=nullptr){
            entry->value = value;
            return true;
        }

        std::lock_guard<std::mutex>lk_guard(freelist_lock_);
        if(freelist_==nullptr){
            return false;
        }
        Entry* new_entry = freelist_;
        freelist_ = freelist_->next;

        new_entry->key = key;
        new_entry->value = value;
        new_entry->prev = nullptr;
        new_entry->next = bucket.entry_list;
        if(bucket.entry_list!=nullptr){
            bucket.entry_list->prev = new_entry;
        }
        bucket.entry_list = new_entry;
        return true;
    }

    //assumes that the bucket lock is held
    bool erase(const KeyType& key){
        Bucket& bucket = buckets_[get_bucket_number(key)];
        Entry* entry = find_entry(bucket,key);
        if(entry==nullptr){
            return false;
        }

        if(bucket.entry_list==entry){
            assert(entry->prev==nullptr);
            bucket.entry_list = entry->next;
        }
        else{
            entry->prev->next = entry->next;
        }
        if(entry->next!=nullptr){
            entry->next->prev = entry->prev;
        }

        std::lock_guard<std::mutex>lk_guard(freelist_lock_);
        entry->next = freelist_;
        freelist_ = entry;
        return true;
    }

    //FOR TESTING AND DEBUGGING
    std::vector<std::pair<KeyType,ValueType>> get_items(){
        std::vector<std::pair<KeyType,ValueType>> items;
        for(int i=0;i<bucket_size;i++){
            Entry* ptr = buckets_[i].entry_list;
            while(ptr!=nullptr){
                items.push_back({ptr->key,ptr->value});
                ptr=ptr->next;
            }
        }
    }
    
private:
    static constexpr uint32_t bucket_size = std::bit_ceil(size);

    struct Entry{
        KeyType key;
        ValueType value;
        Entry* next;
        Entry* prev;
    };

    struct Bucket{
        Lock lock;
        Entry* entry_list;
    };

    //FNV-1a, 64-bit (draft-eastlake-fnv)
    static uint64_t hash(const KeyType& key){
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&key);
        uint64_t h = 0xcbf29ce484222325ULL;
        uint32_t len = sizeof(KeyType);
        while(len--){
            h ^= *p++;
            h *= 0x100000001b3ULL;
        }
        return h;
    }

    static uint32_t get_bucket_number(const KeyType& key){
        return hash(key)%bucket_size;
    }

    Entry* find_entry(Bucket& bucket, const KeyType& key){
        Entry* ptr = bucket.entry_list;
        while(ptr!=nullptr){
            if(std::memcmp(&ptr->key,&key,sizeof(KeyType))==0){
                return ptr;
            }
            ptr = ptr->next;
        }
        return nullptr;
    }

    void init_map(){
        for(uint32_t i=0;i<size;i++){
            mempool_[i].next = (i+1<size) ? &mempool_[i+1] : nullptr;
            mempool_[i].prev = nullptr;
        }
        freelist_ = &mempool_[0];
        for(uint32_t i=0;i<bucket_size;i++){
            buckets_[i].entry_list = nullptr;
        }
    }

    std::array<Entry,size> mempool_;
    std::array<Bucket,bucket_size> buckets_;
    ValueType default_;

    //free to use our own lock - doesn't have to be the templated lock
    std::mutex freelist_lock_;
    Entry* freelist_;
};

#endif