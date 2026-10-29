import argparse
import random
import struct
from scapy.all import Ether, Raw

parser = argparse.ArgumentParser()

parser.add_argument('out')

parser.add_argument('--seed', type=int, default=1)

args = parser.parse_args()

src_macs = []
dst_macs = []
for i in range(7):
    src_macs.append(f"{(5*i)%7:02x}:{(10*i)%7:02x}:{(15*i)%7:02x}:{(20*i)%7:02x}:{(25*i)%7:02x}:02")
    dst_macs.append(f"01:{i%7:02x}:{(2*i)%7:02x}:{(3*i)%7:02x}:{(4*i)%7:02x}:{(5*i)%7:02x}")

rng = random.Random(args.seed)

with open(args.out,"wb") as f:
    ts = 1
    for _ in range(5):
        for src in src_macs:
            for dst in dst_macs:
                data = bytes(Ether(src=src,dst=dst,type=rng.randrange(0x10000))/Raw(b"x"*46))
                f.write(struct.pack('<QI',ts,len(data))+data)
                ts += 1
