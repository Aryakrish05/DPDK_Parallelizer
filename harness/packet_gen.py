# needs better design after logging is added 

from scapy.all import Raw, sendp, hexdump

import argparse

parser = argparse.ArgumentParser()

parser.add_argument('intf')

args = parser.parse_args()

packet = Raw("Hello World!")

packet.show()

hexdump(packet)

sendp(packet, iface = args.intf)