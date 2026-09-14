#ifndef NIC_HPP
#define NIC_HPP

#include <string>
#include <memory>
#include <vector>

#include "Queues.hpp"
#include "NetworkInterface.hpp"
#include "Poller.hpp"
#include "Distributor.hpp"
#include "Packet.hpp"

class NIC{
public:
    explicit NIC(const std::string&,std::shared_ptr<BufferPool>);
    void addRXQueue(std::shared_ptr<RXQueue>);
    void addTXQueue(std::shared_ptr<TXQueue>);
    void run();
private:
    Poller poller_;
    NetworkInterface intf_;
    Distributor distributor_;
    std::shared_ptr<BufferPool> buffer_pool_;
};

NIC::NIC(const std::string& intf_name,std::shared_ptr<BufferPool> pool):intf_(intf_name),buffer_pool_(pool){
    auto receive_action = 
        [this](){
            Packet pkt;
            pkt.buffer = buffer_pool_->acquire();
            if(pkt.buffer==nullptr){
                intf_.discardPacket();
            }
            else if(intf_.recvPacket(pkt)){
                if(!distributor_.distribute(pkt)){
                    buffer_pool_->release(pkt.buffer);
                }
            }   
            else{
                buffer_pool_->release(pkt.buffer);
            }
        };
    poller_.addItem({receive_action});
}

void NIC::addRXQueue(std::shared_ptr<RXQueue> queue_ptr){
    distributor_.addRXQueue(queue_ptr);
}

void NIC::addTXQueue(std::shared_ptr<TXQueue> queue_ptr){
    auto transmit_action = 
        [this,queue_ptr](){
            auto item = queue_ptr->dequeue();
            if(item){
                intf_.sendPacket(*item);
                buffer_pool_->release(item->buffer);
            }
        };
    poller_.addItem({transmit_action});
}

void NIC::run(){
    poller_.run();
}
#endif