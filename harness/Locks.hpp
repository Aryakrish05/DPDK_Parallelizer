#ifndef LOCKS_HPP
#define LOCKS_HPP

#include <cstdint>
#include <mutex>
#include <atomic>

enum class LockingMode{
    READ,
    WRITE
};

class Lock{
public:
    void lock(){
        mtx.lock();
    }
    void unlock(){
        mtx.unlock();
    }
private:
    std::mutex mtx;
};

// currently adopting wait-die strategy, later we'll add other strategies
class TwoPhaseLock{
public:
    bool lock(uint64_t timestamp){
        int cnt = 0;
        while(cnt<max_retries){
            uint64_t holder_timestamp = FREE;
            if(timestamp_.compare_exchange_strong(holder_timestamp,timestamp)){
                return true;
            }
            if(holder_timestamp!=FREE && timestamp>holder_timestamp){
                cnt++;
            }
        }
        return false;
    }
    void unlock(){
        timestamp_.store(FREE);
    }
private:
    static const int max_retries = 100;
    static const uint64_t FREE = INT64_MAX;
    std::atomic<uint64_t> timestamp_{FREE};
};

#endif