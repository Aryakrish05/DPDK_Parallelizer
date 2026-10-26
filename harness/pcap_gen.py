import argparse
import struct
from scapy.all import Raw

parser = argparse.ArgumentParser()

parser.add_argument('out')

parser.add_argument('--n', type=int, default=10)

args = parser.parse_args()

with open(args.out,"wb") as f:
    for i in range(100,args.n+100):
        data = bytes(Raw(f"Hello World{i}"))
        f.write(struct.pack('<QI',i,len(data))+data)
        