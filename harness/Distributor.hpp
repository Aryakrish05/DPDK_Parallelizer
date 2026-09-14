#ifndef DISTRIBUTOR_HPP
#define DISTRIBUTOR_HPP

#include <memory>
#include <stdexcept>
#include "Packet.hpp"
#include "Queues.hpp"

class Distributor{
public:
    void addRXQueue(std::shared_ptr<RXQueue>);
    bool distribute(Packet);
private:
    std::vector<std::shared_ptr<RXQueue>> queues_;
    size_t next_ = 0;
};

void Distributor::addRXQueue(std::shared_ptr<RXQueue>new_queue){
    queues_.push_back(new_queue);
}

bool Distributor::distribute(Packet packet){
    for(size_t count = 0; count<queues_.size(); count++){
        if(queues_[next_]->enqueue(packet)){
            next_=(next_+1)%queues_.size();
            return true;
        }
        next_=(next_+1)%queues_.size();
    }
    return false;
}

#endif