#ifndef QUEUES_HPP
#define QUEUES_HPP

#include <optional>
#include <mutex>
#include <queue>
#include "Packet.hpp"

template<typename T>
class SimpleLockedQueue{
public:
    explicit SimpleLockedQueue(size_t);
    bool tryEnqueue(T);
    std::optional<T> tryDequeue();
private:
    std::queue<T> queue_;
    std::mutex lock_;
    size_t capacity_;
};

template<template<typename> class LockedQueue>
class RXQueueImpl{
public:
    explicit RXQueueImpl(size_t);
    bool enqueue(Packet);
    // returns with as many packets as it is able to collect
    void rxBurst(PacketBatch&,size_t);
private:
    LockedQueue<Packet> queue_;
};

template<template<typename> class LockedQueue>
class TXQueueImpl{
public:
    explicit TXQueueImpl(size_t);
    std::optional<Packet> dequeue();
    // blocks till all the packets in the batch are sent
    void txBurst(PacketBatch&);
private:
    LockedQueue<Packet> queue_;
};


template<typename T>
SimpleLockedQueue<T>::SimpleLockedQueue(size_t capacity):capacity_(capacity){}

template<typename T>
bool SimpleLockedQueue<T>::tryEnqueue(T item){
    std::lock_guard<std::mutex> locked(lock_);
    if(queue_.size()<capacity_){
        queue_.push(item);
        return true;
    }
    return false;
}

template<typename T>
std::optional<T> SimpleLockedQueue<T>::tryDequeue(){
    std::lock_guard<std::mutex> locked(lock_);
    if(!queue_.empty()){
        T item = queue_.front();
        queue_.pop();
        return std::make_optional<T>(item);
    }
    return std::nullopt;
}

template<template<typename> class LockedQueue>
RXQueueImpl<LockedQueue>::RXQueueImpl(size_t capacity):queue_(capacity){}

template<template<typename> class LockedQueue>
bool RXQueueImpl<LockedQueue>::enqueue(Packet packet){
    return queue_.tryEnqueue(packet);
}

template<template<typename> class LockedQueue>
void RXQueueImpl<LockedQueue>::rxBurst(PacketBatch& packet_batch,size_t burst_size){
    for(size_t i=0;i<burst_size;i++){
        auto item = queue_.tryDequeue();
        if(!item){
            break;
        }
        if(!packet_batch.addPacket(*item)){
            break;   
        }
    }
}

template<template<typename> class LockedQueue>
TXQueueImpl<LockedQueue>::TXQueueImpl(size_t capacity):queue_(capacity){}

template<template<typename> class LockedQueue>
std::optional<Packet> TXQueueImpl<LockedQueue>::dequeue(){
    return queue_.tryDequeue();
}

template<template<typename> class LockedQueue>
void TXQueueImpl<LockedQueue>::txBurst(PacketBatch& packet_batch){
    for(size_t i=0;i<packet_batch.size;i++){
        while(!queue_.tryEnqueue(packet_batch.packets[i])){}
    }
}

using RXQueue = RXQueueImpl<SimpleLockedQueue>;
using TXQueue = TXQueueImpl<SimpleLockedQueue>;

#endif