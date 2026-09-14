#ifndef CORE_HPP
#define CORE_HPP

#include <memory>
#include "Packet.hpp"
#include "Program.hpp"
#include "Queues.hpp"

class Core{
public:
    Core(std::shared_ptr<RXQueue>,
         std::shared_ptr<TXQueue>,
         std::shared_ptr<SharedState>,
         std::shared_ptr<BufferPool> buffer_pool);
    void run();

private:
    std::shared_ptr<RXQueue>          rx_queue_;
    std::shared_ptr<TXQueue>          tx_queue_;
    std::shared_ptr<BufferPool>       buffer_pool_;

    std::shared_ptr<SharedState>      shared_state_;
    LocalState                        local_state_;
};

Core::Core(std::shared_ptr<RXQueue> rx,
           std::shared_ptr<TXQueue> tx,
           std::shared_ptr<SharedState> shared,
           std::shared_ptr<BufferPool> pool):
           rx_queue_(rx), tx_queue_(tx), buffer_pool_(pool), shared_state_(shared){}

void Core::run(){
    while(true){
        PacketBatch pkt_batch;
        // in the future add provisions to configure this
        rx_queue_->rxBurst(pkt_batch,MAX_BATCH_SIZE);
        if(pkt_batch.size==0)continue;
        size_t tx_count = 0;
        for(size_t i=0;i<pkt_batch.size;i++){
            Packet& pkt = pkt_batch.packets[i];
            PacketAction pkt_action = process(pkt,*shared_state_,local_state_);
            if(pkt_action == PacketAction::TX){
                pkt_batch.packets[tx_count++] = pkt;
            }
            else{
                buffer_pool_->release(pkt.buffer);
            }
        }
        pkt_batch.size = tx_count;
        tx_queue_->txBurst(pkt_batch);
    }
}
#endif