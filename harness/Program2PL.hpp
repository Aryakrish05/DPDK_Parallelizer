#ifndef PROGRAM_2PL_HPP
#define PROGRAM_2PL_HPP

#include <arpa/inet.h>
#include <iostream>
#include "Packet.hpp"
#include "DSLMaps.hpp"
#include "LockManagers.hpp"
#include "Locks.hpp"

struct MacAddress{
    uint8_t bytes[6];
};

struct __attribute__((packed)) EthernetHeader{
    MacAddress dst;
    MacAddress src;
    uint16_t ether_type;
};

constexpr uint32_t MAX_ENTRIES = 6;
constexpr uint32_t MAX_VALUE_SIZE = 8;

struct SharedState{
    HashMap<MacAddress,uint64_t,1024,TwoPhaseLock,LockKind::AUTO,TwoPhaseLockManager<MAX_ENTRIES,TwoPhaseLock,MAX_VALUE_SIZE>,MAX_VALUE_SIZE> src_counter{"src_counter"};
    HashMap<MacAddress,uint64_t,1024,TwoPhaseLock,LockKind::AUTO,TwoPhaseLockManager<MAX_ENTRIES,TwoPhaseLock,MAX_VALUE_SIZE>,MAX_VALUE_SIZE> dst_counter{"dst_counter"};
    HashMap<EthernetHeader,uint64_t,1024,TwoPhaseLock,LockKind::AUTO,TwoPhaseLockManager<MAX_ENTRIES,TwoPhaseLock,MAX_VALUE_SIZE>,MAX_VALUE_SIZE> hdr_counter{"hdr_counter"};
};

struct LocalState{
    explicit LocalState(CoreContext& ctx):manager(ctx.serializer,ctx.dump_file){}
    TwoPhaseLockManager<MAX_ENTRIES,TwoPhaseLock,MAX_VALUE_SIZE> manager;
};

PacketAction process(Packet& packet, SharedState& shared, LocalState& local){
    TwoPhaseLockManager<MAX_ENTRIES,TwoPhaseLock,MAX_VALUE_SIZE>& mgr = local.manager;
    mgr.setTimestamp(packet.get_timestamp());
    //doesn't modify packet state until after commit
    //if we were to modify packet state, then we would have to take a backup of the packet headers
    while(true){
        char* data = packet.get_data_begin();
        if(data+sizeof(EthernetHeader)>packet.get_data_end()){
            return PacketAction::DROP;
        }   
        EthernetHeader* eth = reinterpret_cast<EthernetHeader*>(data);

        if(!mgr.addAndLock(shared.src_counter.lock_for(eth->src),LockingMode::WRITE)){
            //std::cout<<"Aborting...\n";
            mgr.abort();
            continue;
        }
        uint64_t value;
        shared.src_counter.get(eth->src,value,mgr);
        value = value+eth->dst.bytes[5];
        shared.src_counter.put(eth->src,value,mgr);

        if(!mgr.addAndLock(shared.dst_counter.lock_for(eth->dst),LockingMode::WRITE)){
            //std::cout<<"Aborting...\n";
            mgr.abort();
            continue;
        }

        shared.dst_counter.get(eth->dst,value,mgr);
        value = value+eth->src.bytes[0];
        shared.dst_counter.put(eth->dst,value,mgr);

        if(!mgr.addAndLock(shared.hdr_counter.lock_for(*eth),LockingMode::WRITE)){
            //std::cout<<"Aborting...\n";
            mgr.abort();
            continue;
        }

        shared.hdr_counter.get(*eth,value,mgr);
        value = value+ntohs(eth->ether_type);
        shared.hdr_counter.put(*eth,value,mgr);

        mgr.commit();

        MacAddress tmp = eth->src;
        eth->src = eth->dst;
        eth->dst = tmp;
        return PacketAction::TX;
    }
}

#endif