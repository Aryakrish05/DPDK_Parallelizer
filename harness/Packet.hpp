#ifndef PACKET_HPP
#define PACKET_HPP

#include<vector>
#include<queue>
#include<mutex>
#include<array>
#include<cstdint>

constexpr uint32_t MAX_PACKET_SIZE = 4096;
constexpr uint32_t MAX_BATCH_SIZE = 5;
// in some ways like maximum burst size

//https://doc.dpdk.org/guides/prog_guide/mbuf_lib.html
constexpr uint32_t HEADROOM = 128;

struct PacketBuffer{
    std::array<char,MAX_PACKET_SIZE>bytes;
};

struct Packet{
    PacketBuffer* buffer;
    char* data;
    char* data_end;
    uint64_t timestamp = 0;

    char* get_data_begin(){
        return data;
    }
    char* get_data_end(){
        return data_end;
    }
    //strictly to be used by 2PL - kinda wrong to read it for others
    //https://doc.dpdk.org/guides-23.11/nics/features.html#timestamp-offload
    //https://doc.dpdk.org/guides/sample_app_ug/rxtx_callbacks.html
    //TODO - look at resolution of the timestamps
    uint64_t get_timestamp(){
        return timestamp;
    }
};

enum class PacketAction{
    TX,
    DROP,
};

struct PacketBatch{
    std::array<Packet,MAX_BATCH_SIZE>packets;
    uint32_t size = 0;
    bool addPacket(Packet);
};

bool PacketBatch::addPacket(Packet packet){
    if(size==MAX_BATCH_SIZE)return false;
    packets[size++] = packet;
    return true;
}

struct BufferPool{
public:
    explicit BufferPool(uint32_t);
    PacketBuffer* acquire();
    void release(PacketBuffer*);
    bool allFree();
private:
    std::vector<PacketBuffer> buffers_;
    std::queue<PacketBuffer*> free_buffers_;
    std::mutex lock_;
};

BufferPool::BufferPool(uint32_t num_buffers):buffers_(num_buffers){
    for(uint32_t i=0;i<num_buffers;i++){
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

//need this to know when to stop
bool BufferPool::allFree(){
    std::lock_guard<std::mutex>locked(lock_);
    return free_buffers_.size()==buffers_.size();
}

#endif