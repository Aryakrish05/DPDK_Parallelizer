from file_handler import LogRecord
from typing import List,Dict

#strictly ignores the ordering of packets as there may be interleavings introduced by the NIC
def check_pcap_equality(parallel_pcap:Dict[int,bytes],sequential_pcap:Dict[int,bytes]):
    if(len(parallel_pcap)!=len(sequential_pcap)):
        return False
    for (ts,content) in sequential_pcap.items():
        if(ts not in parallel_pcap):
            return False
        if(parallel_pcap[ts]!=content):
            return False
    return True

#assumes that you pass the sequential implementation or single core implementation
#RUN ON THE SERIALISED OUTPUT
def check_sharing(core_dumps:List[Dict[int,List[LogRecord]]],sequential_dump:Dict[int,List[LogRecord]]):
    sequential_pkt_count = len(sequential_dump)
    parallel_pkt_count = 0
    for dump in core_dumps:
        parallel_pkt_count += len(dump)
    
    if (parallel_pkt_count!=sequential_pkt_count):
        return False
    
    for (ts,records) in sequential_dump.items():
        packet_found = False
        for dump in core_dumps:
            if(ts in dump):
                packet_found = True
                if(dump[ts]!=records):
                    return False
        if(not packet_found):
            return False
    return True

#this should compare the per-core dumps [including the meta dumps]
#and check that there is agreement in the operations done to state
#given a list of dumps - must take the longest thing and compare if things are a prefix of that
#also check against sequential output and log
def check_scr(core_dumps:List[Dict[int,List[LogRecord]]],sequential_dump:Dict[int,List[LogRecord]]):

    longest_dump = {}
    for dump in core_dumps:
        if(len(longest_dump)<len(dump)):
            longest_dump = dump


    if(list(longest_dump.keys()) != list(sequential_dump.keys())):
        return False
    
    for (ts,records) in sequential_dump.items():
        if(longest_dump[ts]!=records):
            return False
        
        
    for dump in core_dumps:
        if (dump == longest_dump):
            continue
        
        last_ts = max(dump,default=0)
        expected_pkt_list = [ts for ts in longest_dump if ts<=last_ts]
        expected_pkt_list.sort()
        if(list(dump.keys()) != expected_pkt_list):
            return False
        
        for (ts,records) in dump.items():
            if(ts not in longest_dump):
                return False
            if(longest_dump[ts]!=records):
                return False
    return True

#this should ensure that no two cores share the same (map,key) pair
#also check that packet outputs are the same as sequential
def check_sharding():
    pass