#include <array>
#include <algorithm>
#include <cassert>
#include <functional>
#include <cstdint>

// max_entries is trivially the maximum number of map calls present in a particular program

//a lock manager for a regular locking attempt
//requires you to add locks one after another 
//then when you call lockAll, it sorts the locks in a pre-defined order
//there may be unlock at any point of time, so unlockOne is a function
//also has unlockAll when the packet gets dropped and all
template<int max_entries,typename Lock>
class LockManager{
public:
    void addLock(Lock* lock){
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
        assert(lock!=nullptr);
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


template<int max_entries,typename TwoPhaseLock,typename MapEntry>
class TwoPhaseLockManager{
public:
    void setTimestamp(uint64_t cur_timestamp){
        timestamp_ = cur_timestamp;
    }
    
    //we only support TwoPhaseLock following the wait-die method with some wait 
    //returns true if lock is already held or lock succeeds, false if it fails, when we should abort
    bool addAndLock(TwoPhaseLock* lock){
        assert(num_locks_ < max_entries);
        assert(lock!=nullptr);
        for(int i=0;i<num_locks_;i++){
            if(lock_list_[i]==lock){
                return true;
            }
        }
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
        assert(lock!=nullptr);
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
        for(int i=0;i<num_locks_;i++){
            if(lock_list_[i]!=nullptr){
                lock_list_[i]->unlock();
            }
        }
        num_locks_ = 0;
        num_dirty_ = 0;
    }

    void commit(){
        std::sort(dirty_list_.begin(),dirty_list_.begin()+num_dirty_,[](MapEntry* e1,MapEntry* e2){
                                                                return reinterpret_cast<std::uintptr_t>(e1)<reinterpret_cast<std::uintptr_t>(e2);
                                                            });
        for(int i=0;i<num_dirty_;i++){
            if(i==0 || (dirty_list_[i]!=dirty_list_[i-1])){
                dirty_list_[i]->swapBuffer();
            }
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
    
    void addDirty(MapEntry* entry){
        assert(num_dirty_<max_entries);
        assert(entry!=nullptr);
        dirty_list_[num_dirty_++] = entry;
    }

private:
    uint64_t timestamp_ = 0;
    std::array<TwoPhaseLock*, max_entries> lock_list_;
    std::array<MapEntry*,max_entries> dirty_list_;
    int num_locks_ = 0;
    int num_dirty_ = 0;
};