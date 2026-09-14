#ifndef PROGRAM_HPP
#define PROGRAM_HPP

#include "Packet.hpp"
#include <iostream>

struct LocalState{
    char bruh[1];
};

struct SharedState{
    char bruh[1];
};

PacketAction process(Packet& packet, SharedState& shared, LocalState& local){
    std::cout<<"Received "<<packet.data_end-packet.data<<" bytes\n";
    return PacketAction::TX;
}

#endif