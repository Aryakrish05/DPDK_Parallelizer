#ifndef LOCKMANAGERS_HPP
#define LOCKMANAGERS_HPP

#include <array>
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <type_traits>
#include "Locks.hpp"

enum class LogAction{
    UNDO,
    COMMIT,
};

enum class Operation{
    INSERT,
    UPDATE,
    ERASE,
    READ // only for testing
};

template<uint32_t max_value_size>
struct LogRecord{
    void (*process) (LogRecord& record, LogAction action);
    void* map;
    void* entry_ptr;
    Operation op;
    uint8_t prev_value[max_value_size];
    void (*dump) (LogRecord& record, std::ostream& out);
    uint8_t new_value[max_value_size];
};

template<typename Map, uint32_t max_value_size>
void process_log(LogRecord<max_value_size>& record, LogAction action){
    Map* map = static_cast<Map*>(record.map);
    if(action==LogAction::COMMIT){
        map->commit(record);
    }
    else if(action==LogAction::UNDO){
        map->undo(record);
    }
}

//it is assumed that we have opened the stream with std::ios::binary
template<typename Map, uint32_t max_value_size>
void dump_log(LogRecord<max_value_size>& record, std::ostream& out){
    static_cast<Map*>(record.map)->dump(record,out);
}

//record format: [name size u8][name][op u8][key size u32][key][value size u32][prev value][new value]
template<typename KeyType, typename ValueType, uint32_t max_value_size>
void write_record(std::ostream& out, std::string& name, KeyType& key, LogRecord<max_value_size>& record){
    assert(name.size()<=UINT8_MAX);
    uint8_t name_size = static_cast<uint8_t>(name.size());
    out.write(reinterpret_cast<char*>(&name_size),sizeof(uint8_t));
    out.write(name.data(),name_size);

    uint8_t op = static_cast<uint8_t>(record.op);
    out.write(reinterpret_cast<char*>(&op),sizeof(uint8_t));

    uint32_t key_size = sizeof(KeyType);
    out.write(reinterpret_cast<char*>(&key_size),sizeof(uint32_t));
    out.write(reinterpret_cast<char*>(&key),sizeof(KeyType));

    uint32_t value_size = sizeof(ValueType);
    out.write(reinterpret_cast<char*>(&value_size),sizeof(uint32_t));
    out.write(reinterpret_cast<char*>(record.prev_value),sizeof(ValueType));
    out.write(reinterpret_cast<char*>(record.new_value),sizeof(ValueType));
}

template<typename Map, uint32_t max_value_size, typename ValueType>
LogRecord<max_value_size> make_log_record(Map* map, void* entry_ptr, Operation op, const ValueType& prev_value, const ValueType& new_value){
    static_assert(sizeof(ValueType)<=max_value_size);
    static_assert(std::is_trivially_copyable_v<ValueType>);
    LogRecord<max_value_size> record = {
        .process = &process_log<Map,max_value_size>,
        .map = map,
        .entry_ptr = entry_ptr,
        .op = op,
        .dump = &dump_log<Map,max_value_size>,
    };
    std::memcpy(record.prev_value,&prev_value,sizeof(ValueType));
    std::memcpy(record.new_value,&new_value,sizeof(ValueType));
    return record;
}

// max_entries is trivially the maximum number of map calls present in a particular program

//a lock manager for a regular locking attempt
//requires you to add locks one after another 
//then when you call lockAll, it sorts the locks in a pre-defined order
//there may be unlock at any point of time, so unlockOne is a function (TODO - ANALYSE THIS AND USE IN FURTHER VERSIONS)
//also has unlockAll when the packet gets dropped and all
template<uint32_t max_entries,typename Lock,uint32_t max_value_size>
class LockManager{
public:
    void setTimestamp(uint64_t cur_timestamp){
        timestamp_ = cur_timestamp;
    }

    void addLock(Lock* lock, LockingMode mode){
        assert(num_locks_<max_entries && !locked);
        lock_list_[num_locks_++] = lock;
    }

    //only used for debugging, records are never undone
    void addLogRecord(const LogRecord<max_value_size>& record){
        assert(num_log_records_<max_entries);
        log_record_list_[num_log_records_++] = record;
    }

    void dump(std::ostream& out){
        out.write(reinterpret_cast<char*>(&timestamp_),sizeof(uint64_t));
        out.write(reinterpret_cast<char*>(&num_log_records_),sizeof(uint32_t));
        for(uint32_t i=0;i<num_log_records_;i++){
            log_record_list_[i].dump(log_record_list_[i],out);
        }
    }
    void lockAll(){
        std::sort(lock_list_.begin(),lock_list_.begin()+num_locks_,[](Lock* lk1,Lock* lk2){
                                                                        return reinterpret_cast<std::uintptr_t>(lk1)<reinterpret_cast<std::uintptr_t>(lk2);
                                                                    });
        for(uint32_t i=0;i<num_locks_;i++){
            if(lock_list_[i]!=nullptr && (i==0 || (lock_list_[i-1]!=lock_list_[i]))){
                lock_list_[i]->lock();
            }
        }
        locked = true;
    }
    /*
    void unlockOne(Lock* lock){
        assert(locked);
        if(lock==nullptr){
            return;
        }
        bool held = false;
        for(uint32_t i=0;i<num_locks_;i++){
            if(lock_list_[i]==lock){
                held = true;
                lock_list_[i] = nullptr;
            }
        }
        assert(held);
        lock->unlock();
    }
    */
    void unlockAll(){
        assert(locked);
        //throw this away eventually and modify maps
        //this is needed only because we are using the logging for the tests
        for(uint32_t i=0;i<num_log_records_;i++){
            log_record_list_[i].process(log_record_list_[i],LogAction::COMMIT);
        }
        // assumes that lockAll was called and the array is sorted except for nullptrs in bw
        for(uint32_t i=0;i<num_locks_;i++){
            if(lock_list_[i]!=nullptr && (i==0 || (lock_list_[i-1]!=lock_list_[i]))){
                lock_list_[i]->unlock();
            }
        }
        num_locks_ = 0;
        locked = false;
        num_log_records_ = 0;
    }
private:
    std::array<Lock*, max_entries> lock_list_;
    uint32_t num_locks_ = 0;
    bool locked = false;
    std::array<LogRecord<max_value_size>,max_entries> log_record_list_;
    uint32_t num_log_records_ = 0;
    uint64_t timestamp_ = 0;
};

//a lock manager for 2 phase locking
//we need to maintain which locations have memory writes to them
//since aborting is a possibility, allowing singular unlocks on memlocs which have been written to
//we shall unlock only those which have not been written to (TODO - CHECK THIS AND USE IN LATER VERSIONS)
//we shall have an abort function which unlocks all the locks
//we shall also have a commit function which unlocks all the locks and commits the log


template<uint32_t max_entries,typename TwoPhaseLock,uint32_t max_value_size>
class TwoPhaseLockManager{
public:
    void setTimestamp(uint64_t cur_timestamp){
        timestamp_ = cur_timestamp;
    }
    
    //we only support TwoPhaseLock following the wait-die method with some wait 
    //returns true if lock is already held or lock succeeds, false if it fails, when we should abort
    bool addAndLock(TwoPhaseLock* lock, LockingMode mode){
        assert(timestamp_!=0);
        if(lock==nullptr){
            return true;
        }

        for(uint32_t i=0;i<num_locks_;i++){
            if(lock_list_[i]==lock){
                return true;
            }
        }
        assert(num_locks_ < max_entries);
        if(lock->lock(timestamp_)){
            lock_list_[num_locks_++] = lock;
            return true;
        }
        return false;
    }
    

    /*
    void unlockReadOnly(TwoPhaseLock* lock){
        //full trust the compiler to unlock only the readonly locks
        //i have no way of detecting it
        //also we have no duplicates due to the way addAndLock is done
        if(lock==nullptr){
            return;
        }
        bool locked = false;
        for(uint32_t i=0;i<num_locks_;i++){
            if(lock_list_[i]==lock){
                lock->unlock();
                lock_list_[i] = nullptr;
                locked = true;
                break;
            }
        }
        assert(locked);
        return;
    }
    */

    void abort(){
        for(uint32_t i=num_log_records_;i-->0;){
            log_record_list_[i].process(log_record_list_[i],LogAction::UNDO);
        }
        for(uint32_t i=0;i<num_locks_;i++){
            if(lock_list_[i]!=nullptr){
                lock_list_[i]->unlock();
            }
        }
        num_locks_ = 0;
        num_log_records_ = 0;
    }

    //log is processed before unlocking, erased nodes must be freed while still locked
    void commit(){
        for(uint32_t i=0;i<num_log_records_;i++){
            log_record_list_[i].process(log_record_list_[i],LogAction::COMMIT);
        }

        for(uint32_t i=0;i<num_locks_;i++){
            if(lock_list_[i]!=nullptr){
                lock_list_[i]->unlock();
            }
        }
        
        timestamp_ = 0;
        num_locks_ = 0;
        num_log_records_ = 0;
    }
    
    void addLogRecord(const LogRecord<max_value_size>& record){
        assert(num_log_records_<max_entries);
        log_record_list_[num_log_records_++] = record;
    }

    //call before commit, the log is cleared by commit and abort
    void dump(std::ostream& out){
        out.write(reinterpret_cast<char*>(&timestamp_),sizeof(uint64_t));
        out.write(reinterpret_cast<char*>(&num_log_records_),sizeof(uint32_t));
        for(uint32_t i=0;i<num_log_records_;i++){
            log_record_list_[i].dump(log_record_list_[i],out);
        }
    }

private:
    uint64_t timestamp_ = 0;
    std::array<TwoPhaseLock*, max_entries> lock_list_;
    std::array<LogRecord<max_value_size>,max_entries> log_record_list_;
    uint32_t num_locks_ = 0;
    uint32_t num_log_records_ = 0;
};

#endif