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

map<uint32_t, struct counter, key, 1, array> count1 { pass; };

map<uint32_t, struct counter, key, 1, array> count2 { pass; };

int process(struct phv phv){
    Extract(phv.eth);
    if(count1.rmw(0,rmw_add,phv.eth.proto>>46)){
        if(count2.rmw(0,rmw_add,eth.proto>>44)){
            pass;
        }
        else{
            pass;
        }
    }
    else{
        pass;
    }
    return DROP;
}