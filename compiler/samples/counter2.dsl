struct mac_addr{
    uint8_t bytes[6];
};

struct ethhdr{
    struct mac_addr src_mac;
    struct mac_addr dst_mac;
    uint16_t proto;
};

struct phv{
    struct ethhdr eth;
};

map<uint32_t, uint32_t, auto, 1, array> count1;

map<uint32_t, uint32_t, auto, 1, array> count2;

int process(struct phv phv){
    Extract(phv.eth);
    count1.rmw(0,rmw_add,phv.eth.src_mac.bytes[0]>>6);
    count2.rmw(0,rmw_add,phv.eth.dst_mac.bytes[0]>>4);
    return DROP;
}