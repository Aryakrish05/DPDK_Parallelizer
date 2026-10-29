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
#include <mutex>
#include <fstream>
#include <stdexcept>
#include <memory>
#include "Locks.hpp"
#include "Log.hpp"

class Serializer{
public:
    explicit Serializer(std::string file){
        out_.open(file);
        if(!out_.is_open()){
            throw std::runtime_error("failed to open serialization file");
        }
    }
    void serialize(uint64_t timestamp){
        std::lock_guard<std::mutex>lk_guard(lock_);
        out_<<timestamp<<"\n"<<std::flush;
    }
private:
    std::mutex lock_;
    std::ofstream out_;
};

struct CoreContext{
    uint32_t core_id;
    std::shared_ptr<Serializer> serializer;
    std::string dump_file;
};

// max_entries is trivially the maximum number of map calls present in a particular program

//a lock manager for a regular locking attempt
//requires you to add locks one after another 
//then when you call lockAll, it sorts the locks in a pre-defined order
//there may be unlock at any point of time, so unlockOne is a function (TODO - ANALYSE THIS AND USE IN FURTHER VERSIONS)
//also has unlockAll when the packet gets dropped and all
template<uint32_t max_entries,typename Lock,uint32_t max_value_size>
class LockManager{
public:
    LockManager(std::shared_ptr<Serializer> serializer_ptr,std::string dump_file):serializer_(serializer_ptr){
        dump_stream_.open(dump_file,std::ios::binary);
        if(!dump_stream_.is_open()){
            throw std::runtime_error("failed to open cpu specific dump streams");
        }
    }
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

    void commit(){
        serializer_->serialize(timestamp_);
        dump(dump_stream_);
        dump_stream_.flush();
        unlockAll();
    }

private:
    std::array<Lock*, max_entries> lock_list_;
    uint32_t num_locks_ = 0;
    bool locked = false;
    std::array<LogRecord<max_value_size>,max_entries> log_record_list_;
    uint32_t num_log_records_ = 0;
    uint64_t timestamp_ = 0;
    std::shared_ptr<Serializer> serializer_;
    std::ofstream dump_stream_;
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
    
    TwoPhaseLockManager(std::shared_ptr<Serializer> serializer_ptr,std::string dump_file):serializer_(serializer_ptr){
        dump_stream_.open(dump_file,std::ios::binary);
        if(!dump_stream_.is_open()){
            throw std::runtime_error("failed to open cpu specific dump streams");
        }
    }
    
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
        serializer_->serialize(timestamp_);
        dump(dump_stream_);
        dump_stream_.flush();
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
    std::shared_ptr<Serializer> serializer_;
    std::ofstream dump_stream_;
};

#endif