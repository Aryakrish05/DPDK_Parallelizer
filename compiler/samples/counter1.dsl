struct ethhdr{
    uint48_t src_mac;
    uint48_t dst_mac;
    uint16_t proto;
};

struct phv{
    struct ethhdr eth;
};

map<uint32_t, uint32_t, key, 1, array> count;

int process(struct phv phv){
    Extract(phv.eth);
    count.rmw(0,rmw_add,1);
    return DROP;
}