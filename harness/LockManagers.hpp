#ifndef LOCKMANAGERS_HPP
#define LOCKMANAGERS_HPP

#include <array>
#include <algorithm>
#include <cassert>
#include <functional>
#include <cstdint>
#include "Locks.hpp"

// max_entries is trivially the maximum number of map calls present in a particular program

//a lock manager for a regular locking attempt
//requires you to add locks one after another 
//then when you call lockAll, it sorts the locks in a pre-defined order
//there may be unlock at any point of time, so unlockOne is a function
//also has unlockAll when the packet gets dropped and all
template<int max_entries,typename Lock>
class LockManager{
public:
    void addLock(Lock* lock, LockingMode mode){
        assert(num_locks_<max_entries && !locked);
        lock_list_[num_locks_++] = lock;
    }
    void lockAll(){
        std::sort(lock_list_.begin(),lock_list_.begin()+num_locks_,[](Lock* lk1,Lock* lk2){
                                                                        return reinterpret_cast<std::uintptr_t>(lk1)<reinterpret_cast<std::uintptr_t>(lk2);
                                                                    });
        for(int i=0;i<num_locks_;i++){
            if(lock_list_[i]!=nullptr && (i==0 || (lock_list_[i-1]!=lock_list_[i]))){
                lock_list_[i]->lock();
            }
        }
        locked = true;
    }
    void unlockOne(Lock* lock){
        assert(locked);
        if(lock==nullptr){
            return;
        }
        bool held = false;
        for(int i=0;i<num_locks_;i++){
            if(lock_list_[i]==lock){
                held = true;
                lock_list_[i] = nullptr;
            }
        }
        assert(held);
        lock->unlock();
    }
    void unlockAll(){
        assert(locked);
        // assumes that lockAll was called and the array is sorted except for nullptrs in bw
        for(int i=0;i<num_locks_;i++){
            if(lock_list_[i]!=nullptr && (i==0 || (lock_list_[i-1]!=lock_list_[i]))){
                lock_list_[i]->unlock();
            }
        }
        num_locks_ = 0;
        locked = false;
    }
private:
    std::array<Lock*, max_entries> lock_list_;
    int num_locks_ = 0;
    bool locked = false;
};

//a lock manager for 2 phase locking
//we need to maintain which locations have memory writes to them
//since aborting is a possibility, allowing singular unlocks on memlocs which have been written to
//we shall unlock only those which have not been written to
//we shall have an abort function which unlocks all the locks
//we shall also have a commit function which unlocks all the locks and switches the copy bit


template<int max_entries,typename TwoPhaseLock>
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

        for(int i=0;i<num_locks_;i++){
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
    

    void unlockReadOnly(TwoPhaseLock* lock){
        //full trust the compiler to unlock only the readonly locks
        //i have no way of detecting it
        //also we have no duplicates due to the way addAndLock is done
        if(lock==nullptr){
            return;
        }
        bool locked = false;
        for(int i=0;i<num_locks_;i++){
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

    void abort(){
        for(int i=0;i<num_dirty_;i++){
            dirty_list_[i][1] = false;
        }
        for(int i=0;i<num_locks_;i++){
            if(lock_list_[i]!=nullptr){
                lock_list_[i]->unlock();
            }
        }
        num_locks_ = 0;
        num_dirty_ = 0;
    }

    void commit(){
        for(int i=0;i<num_dirty_;i++){
            dirty_list_[i][0] = !dirty_list_[i][0];
            dirty_list_[i][1] = false;
        }

        for(int i=0;i<num_locks_;i++){
            if(lock_list_[i]!=nullptr){
                lock_list_[i]->unlock();
            }
        }
        
        timestamp_ = 0;
        num_locks_ = 0;
        num_dirty_ = 0;
    }
    
    void addDirty(bool* meta){
        assert(meta!=nullptr);
        if(meta[1]){
            return;
        }
        assert(num_dirty_<max_entries);
        meta[1] = true;
        dirty_list_[num_dirty_++] = meta;
    }

private:
    uint64_t timestamp_ = 0;
    std::array<TwoPhaseLock*, max_entries> lock_list_;

    //contains the pointers to bool meta[2] for each entry
    //for every map entry, 
    //meta[0] refers to the current active entry
    //meta[1] refers to the dirty bit (needed for the core to find out where to read from)
    //also, this is enough as we are allowing only whole entry writes into the maps
    //if we were allowing field wise manipulation, we need to have field wise copies and metadata
    std::array<bool*,max_entries> dirty_list_;
    int num_locks_ = 0;
    int num_dirty_ = 0;
};

#endif