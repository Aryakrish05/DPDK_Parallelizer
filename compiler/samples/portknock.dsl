// translated from bpf-profile portknock v0
// stuff from imported headers defined 

enum state {
  CLOSED_0 = 0,
  CLOSED_1,
  CLOSED_2,
  OPEN,
};

struct phv{
    struct ethhdr eth;
    struct iphdr ip;
    struct udphdr udp;
    struct tcphdr tcp;
} ;


struct array_elem {
  uint32_t state;
};

map<uint32_t, struct array_elem, global, 1, array> port_state { pass; };

int process(struct phv phv){
    uint16_t dport;
    Extract(phv.eth);
    if(phv.eth.h_proto != ETH_P_IP){
        return DROP;
    }
    Extract(phv.ip);
    if(phv.ip.protocol == IPPROTO_UDP){
        Extract(phv.udp);
        dport = phv.udp.dest;
    }
    else if(phv.ip.protocol == IPPROTO_TCP){
        Extract(phv.tcp);
        dport = phv.tcp.dest;
    }
    else{
        return DROP;
    }

    if(struct array_elem value = port_state.get(0)){
        if(value.state == OPEN){
            return FORWARD;
        }
        if ((value.state == CLOSED_0) && (dport == PORT_1)) {
            value.state = CLOSED_1;  
        } else if ((value.state == CLOSED_1) && (dport == PORT_2)) {
            value.state = CLOSED_2;
        } else if ((value.state == CLOSED_2) && (dport == PORT_3)) {
            value.state = OPEN;
        } else {
            value.state = CLOSED_0;
        }
        if(port_state.put(0,value)){
            pass;
        }
        else{
            pass;
        }
    }
    else{
        pass;
    }
    
    // need to add stuff like swap_src_dst_mac(data);
    return FORWARD;
}