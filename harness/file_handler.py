from dataclasses import dataclass

Operation = ["INSERT","UPDATE","ERASE","READ"]

@dataclass
class LogRecord:
    map_name: str
    op: str
    key: bytes
    prev_value: bytes
    new_value: bytes
    
def read_uint(f,n):
    return int.from_bytes(f.read(n),"little")

def read_dump_file(filename):
    with open(filename,"rb") as f:
        #timestamp to list of operations
        pkt_dump = {}
        while (header:=f.read(8)):
            ts = int.from_bytes(header,"little")
            no_records = read_uint(f,4)
            records = []
            for _ in range(no_records):
                map_name = f.read(read_uint(f,1)).decode()
                op = Operation[read_uint(f,1)]
                key = f.read(read_uint(f,4))
                value_sz = read_uint(f,4)
                prev_value = f.read(value_sz)
                new_value = f.read(value_sz)
                records.append(LogRecord(map_name,op,key,prev_value,new_value))
            assert(ts not in pkt_dump)
            pkt_dump[ts] = records
        return pkt_dump

def read_serial_file(filename):
    with open(filename) as f:
        return [int(line) for line in f]

def read_pcap_file(filename):
    with open(filename,"rb") as f:
        sent_packets = {}
        while(header:=f.read(8)):
            ts = int.from_bytes(header,"little")
            assert(ts not in sent_packets)
            sent_packets[ts] = f.read(read_uint(f,4))
        return sent_packets
    
def write_pcap_file_in_order(filename,order,ts_to_packet):
    with open(filename,"wb") as f:
        for ts in order:
            f.write(int.to_bytes(ts,8,"little"))
            f.write(int.to_bytes(len(ts_to_packet[ts]),4,"little"))
            f.write(ts_to_packet[ts])
