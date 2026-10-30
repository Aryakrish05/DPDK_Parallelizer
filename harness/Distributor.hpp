#ifndef DISTRIBUTOR_HPP
#define DISTRIBUTOR_HPP

#include <memory>
#include <stdexcept>
#include "Packet.hpp"
#include "Queues.hpp"
#include <cstring>
#include <cassert>
#include <vector>
#ifdef PROGRAM
#include PROGRAM
#else
#include "Program.hpp"
#endif

#ifdef SCR
//The distributor just distributes one packet per queue after adding metadata to it
//Right now, Meta contains a timestamp(not really needed - but there for now)

//fill_metadata should be added in the program file while running SCR
class Distributor{
public:
    void addRXQueue(std::shared_ptr<RXQueue>new_queue){
        queues_.push_back(new_queue);
        history_.resize(queues_.size());
        //add room for ethernet also later
        assert(HEADROOM >= sizeof(Meta)*(queues_.size()-1));
    }
    bool distribute(Packet packet){
        assert(packet.timestamp!=0);
        fill_metadata(history_[next_],packet);
        uint32_t num_queues = queues_.size();
        for(uint32_t i=(next_+num_queues-1)%num_queues;i!=next_;i=(i-1+num_queues)%num_queues){
            packet.data -= sizeof(Meta);
            std::memcpy(packet.data,&history_[i],sizeof(Meta));   
        }
        while(!queues_[next_]->enqueue(packet));
        next_ = (next_+1)%num_queues;
        //no packet is dropped by the distributor, in this design
        return true;
    }

private:
    std::vector<std::shared_ptr<RXQueue>> queues_;
    std::vector<Meta> history_;
    uint32_t next_ = 0;
};
#else
class Distributor{
public:
    void addRXQueue(std::shared_ptr<RXQueue>);
    bool distribute(Packet);
private:
    std::vector<std::shared_ptr<RXQueue>> queues_;
    uint32_t next_ = 0;
    uint64_t timestamp_ = 0;
};

void Distributor::addRXQueue(std::shared_ptr<RXQueue>new_queue){
    queues_.push_back(new_queue);
}

bool Distributor::distribute(Packet packet){
    if(packet.timestamp==0){
        packet.timestamp = ++timestamp_;
    }
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

#endif