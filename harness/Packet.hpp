#ifndef PACKET_HPP
#define PACKET_HPP

#include<vector>
#include<queue>
#include<mutex>
#include<array>

constexpr size_t MAX_PACKET_SIZE = 4096;
constexpr size_t MAX_BATCH_SIZE = 5;
// in some ways like maximum burst size

struct PacketBuffer{
    std::array<char,MAX_PACKET_SIZE>bytes;
};

struct Packet{
    PacketBuffer* buffer;
    char* data;
    char* data_end;
};

enum PacketAction{
    TX,
    DROP,
};

struct PacketBatch{
    std::array<Packet,MAX_BATCH_SIZE>packets;
    size_t size = 0;
    bool addPacket(Packet);
};

bool PacketBatch::addPacket(Packet packet){
    if(size==MAX_BATCH_SIZE)return false;
    packets[size++] = packet;
    return true;
}

struct BufferPool{
public:
    explicit BufferPool(size_t);
    PacketBuffer* acquire();
    void release(PacketBuffer*);
private:
    std::vector<PacketBuffer> buffers_;
    std::queue<PacketBuffer*> free_buffers_;
    std::mutex lock_;
};

BufferPool::BufferPool(size_t num_buffers):buffers_(num_buffers){
    for(size_t i=0;i<num_buffers;i++){
        free_buffers_.push(&buffers_[i]);
    }
}

PacketBuffer* BufferPool::acquire(){
    std::lock_guard<std::mutex>locked(lock_);
    if(free_buffers_.empty()){
        return nullptr;
    }
    PacketBuffer* buf = free_buffers_.front();
    free_buffers_.pop();
    return buf;
}
void BufferPool::release(PacketBuffer* free_buf){
    std::lock_guard<std::mutex>locked(lock_);
    free_buffers_.push(free_buf);
}

#endif