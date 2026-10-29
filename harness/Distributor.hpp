#ifndef DISTRIBUTOR_HPP
#define DISTRIBUTOR_HPP

#include <memory>
#include <stdexcept>
#include "Packet.hpp"
#include "Queues.hpp"
#ifdef PROGRAM
#include PROGRAM
#else
#include "Program.hpp"
#endif

// add compile guards to this based on SCR and SHARDING
// the metadata struct and the function for computing metadata will be accessed under these guards
// similarly for SHARDING - we will access the hash and distribution function under the appropriate guards

class Distributor{
public:
    void addRXQueue(std::shared_ptr<RXQueue>);
    bool distribute(Packet);
private:
    std::vector<std::shared_ptr<RXQueue>> queues_;
    uint32_t next_ = 0;
    uint64_t timestamp_ = 0;
#ifdef SCR 
    //assume that the number of cores are known
    std::array<Meta,num_cores> history;
#endif
};

void Distributor::addRXQueue(std::shared_ptr<RXQueue>new_queue){
    queues_.push_back(new_queue);
}

bool Distributor::distribute(Packet packet){
    if(packet.timestamp==0){
        packet.timestamp = ++timestamp_;
    }
#ifdef SCR
    //we shall use the timestamp as the SCR seqnum for now, although we are not doing anything with it
    //assume Meta is imported
    
#endif
    for(uint32_t count = 0; count<queues_.size(); count++){
        if(queues_[next_]->enqueue(packet)){
            next_=(next_+1)%queues_.size();
            return true;
        }
        next_=(next_+1)%queues_.size();
    }
    return false;
}

#endif