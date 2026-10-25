struct ethhdr{
    uint48_t src_mac;
    uint48_t dst_mac;
    uint16_t proto;
};

struct phv{
    struct ethhdr eth;
};

map<uint32_t, uint32_t, key, 1, array> count1;

map<uint32_t, uint32_t, key, 1, array> count2;

int process(struct phv phv){
    Extract(phv.eth);
    count1.rmw(0,rmw_add,phv.eth.src_mac>>46);
    count2.rmw(0,rmw_add,phv.eth.dst_mac>>44);
    return DROP;
}