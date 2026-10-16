struct ethhdr{
    uint48_t src_mac;
    uint48_t dst_mac;
    uint16_t proto;
};

struct phv{
    struct ethhdr eth;
};

struct counter {
  uint32_t count;
};

map<uint32_t, struct counter, key, 1, array> count;

int process(struct phv phv){
    Extract(phv.eth);
    if(count.rmw(0,rmw_add,1)){ }
    return DROP;
}