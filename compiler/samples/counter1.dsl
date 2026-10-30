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

map<uint32_t, uint32_t, auto, 1, array> count;

int process(struct phv phv){
    Extract(phv.eth);
    count.rmw(0,rmw_add,1);
    return DROP;
}