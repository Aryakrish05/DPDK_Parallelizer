struct mac_addr{
    uint8_t bytes[6];
};

struct ethhdr{
    struct mac_addr h_dest;
    struct mac_addr h_source;
    uint16_t h_proto;
};

struct phv{
    struct ethhdr eth;
};

map<struct mac_addr, uint64_t, auto, 1024, hash> src_counter;
map<struct mac_addr, uint64_t, auto, 1024, hash> dst_counter;
map<struct ethhdr, uint64_t, auto, 1024, hash> hdr_counter;

int process(struct phv phv){
    Extract(phv.eth);

    uint64_t value;
    src_counter.get(phv.eth.h_source, value);
    value = value + phv.eth.h_dest.bytes[5];
    src_counter.put(phv.eth.h_source, value);

    dst_counter.get(phv.eth.h_dest, value);
    value = value + phv.eth.h_source.bytes[0];
    dst_counter.put(phv.eth.h_dest, value);

    hdr_counter.get(phv.eth, value);
    value = value + ntohs(phv.eth.h_proto);
    hdr_counter.put(phv.eth, value);

    struct mac_addr tmp = phv.eth.h_source;
    phv.eth.h_source = phv.eth.h_dest;
    phv.eth.h_dest = tmp;
    return FORWARD;
}
