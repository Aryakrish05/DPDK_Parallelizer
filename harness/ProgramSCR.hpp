#ifndef PROGRAM_SCR_HPP
#define PROGRAM_SCR_HPP

#define SCR

#include <arpa/inet.h>
#include <cstring>
#include "Packet.hpp"
#include "DSLMaps.hpp"
#include "LockManagers.hpp"
#include "OtherManagers.hpp"
#include "Locks.hpp"

struct MacAddress{
    uint8_t bytes[6];
};

struct __attribute__((packed)) EthernetHeader{
    MacAddress dst;
    MacAddress src;
    uint16_t ether_type;
};

struct Meta{
    uint64_t ts;
    EthernetHeader eth;
};

constexpr uint32_t MAX_ENTRIES = 6;
constexpr uint32_t MAX_VALUE_SIZE = 8;

struct SharedState{
};

struct LocalState{
    explicit LocalState(CoreContext& ctx):num_meta(ctx.num_cores-1),manager(ctx.dump_file){}
    uint32_t num_meta;
    SCRManager<MAX_ENTRIES,MAX_VALUE_SIZE> manager;
    HashMap<MacAddress,uint64_t,1024,Lock,LockKind::AUTO,SCRManager<MAX_ENTRIES,MAX_VALUE_SIZE>,MAX_VALUE_SIZE> src_counter{"src_counter"};
    HashMap<MacAddress,uint64_t,1024,Lock,LockKind::AUTO,SCRManager<MAX_ENTRIES,MAX_VALUE_SIZE>,MAX_VALUE_SIZE> dst_counter{"dst_counter"};
    HashMap<EthernetHeader,uint64_t,1024,Lock,LockKind::AUTO,SCRManager<MAX_ENTRIES,MAX_VALUE_SIZE>,MAX_VALUE_SIZE> hdr_counter{"hdr_counter"};
};

void fill_metadata(Meta& meta, Packet& packet){
    meta = Meta{};
    char* data = packet.get_data_begin();
    if(data+sizeof(EthernetHeader)>packet.get_data_end()){
        return;
    }
    meta.ts = packet.get_timestamp();
    std::memcpy(&meta.eth,data,sizeof(EthernetHeader));
}

void catch_up(Packet& packet, LocalState& local){
    SCRManager<MAX_ENTRIES,MAX_VALUE_SIZE>& mgr = local.manager;
    for(uint32_t i=0;i<local.num_meta;i++){
        Meta meta;
        std::memcpy(&meta,packet.get_data_begin(),sizeof(Meta));
        //for now have a hacky way of ensuring output packet doesn't contain meta
        //need to come up with a neater solution later 
        packet.data += sizeof(Meta);
        //find a neater way to do this check also 
        //Ask Sir
        if(meta.ts==0){
            continue;
        }

        mgr.setTimestamp(meta.ts);
        uint64_t value;
        local.src_counter.get(meta.eth.src,value,mgr);
        value = value+meta.eth.dst.bytes[5];
        local.src_counter.put(meta.eth.src,value,mgr);

        local.dst_counter.get(meta.eth.dst,value,mgr);
        value = value+meta.eth.src.bytes[0];
        local.dst_counter.put(meta.eth.dst,value,mgr);

        local.hdr_counter.get(meta.eth,value,mgr);
        value = value+ntohs(meta.eth.ether_type);
        local.hdr_counter.put(meta.eth,value,mgr);
        mgr.commit();
    }
}

PacketAction process(Packet& packet, SharedState& shared, LocalState& local){
    catch_up(packet,local);

    char* data = packet.get_data_begin();
    if(data+sizeof(EthernetHeader)>packet.get_data_end()){
        return PacketAction::DROP;
    }
    EthernetHeader* eth = reinterpret_cast<EthernetHeader*>(data);

    SCRManager<MAX_ENTRIES,MAX_VALUE_SIZE>& mgr = local.manager;
    mgr.setTimestamp(packet.get_timestamp());
    uint64_t value;
    local.src_counter.get(eth->src,value,mgr);
    value = value+eth->dst.bytes[5];
    local.src_counter.put(eth->src,value,mgr);

    local.dst_counter.get(eth->dst,value,mgr);
    value = value+eth->src.bytes[0];
    local.dst_counter.put(eth->dst,value,mgr);

    local.hdr_counter.get(*eth,value,mgr);
    value = value+ntohs(eth->ether_type);
    local.hdr_counter.put(*eth,value,mgr);
    mgr.commit();

    MacAddress tmp = eth->src;
    eth->src = eth->dst;
    eth->dst = tmp;
    return PacketAction::TX;
}

#endif
