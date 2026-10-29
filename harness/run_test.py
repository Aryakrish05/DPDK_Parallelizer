# GenAI used for writing this script

#usage: python3 run_test.py <generator.py> [--sequential P] [--sharing P] [--sharding P] [--scr P] [--max-cores N] [--seed S]
#       python3 run_test.py --clear-results
#every run keeps its files in results/<run name>/

import argparse
import os
import shutil
import subprocess
import sys

from file_handler import read_pcap_file, read_serial_file, read_dump_file, write_pcap_file_in_order
from checker import check_pcap_equality, check_sharing

HARNESS_DIR = os.path.dirname(os.path.abspath(__file__))
RESULTS_DIR = os.path.join(HARNESS_DIR, "results")

parser = argparse.ArgumentParser()
parser.add_argument("generator", nargs="?")
parser.add_argument("--clear-results", action="store_true")
parser.add_argument("--sequential")
parser.add_argument("--sharing")
parser.add_argument("--sharding")
parser.add_argument("--scr")
parser.add_argument("--max-cores", type=int, default=3)
parser.add_argument("--seed", type=int, default=1)
args = parser.parse_args()

if args.clear_results:
    shutil.rmtree(RESULTS_DIR, ignore_errors=True)
    print("cleared " + RESULTS_DIR)
    sys.exit(0)

if args.generator is None:
    parser.error("the generator script is required unless --clear-results is given")

os.makedirs(RESULTS_DIR, exist_ok=True)


def build(strategy, program):
    binary = os.path.join(RESULTS_DIR, "harness_" + strategy)
    subprocess.run([os.path.join(HARNESS_DIR, "build.sh"), os.path.abspath(program), binary], check=True)
    return binary


def run_harness(binary, input_file, run_name, cores, num_packets):
    run_dir = os.path.join(RESULTS_DIR, run_name)
    os.makedirs(run_dir, exist_ok=True)
    output_file = os.path.join(run_dir, "out.pcap")
    command = [binary,
               "-p", input_file + "," + output_file,
               "-c", str(cores),
               #queue size: large enough to hold every packet, as the NIC waits on a full queue instead of dropping
               "-q", str(num_packets + 1),
               "-d", run_dir + "/"]
    subprocess.run(command, check=True, timeout=60)

    sent_packets = read_pcap_file(output_file)
    serial_order = read_serial_file(os.path.join(run_dir, "serial.csv"))
    dumps = []
    for core in range(cores):
        dumps.append(read_dump_file(os.path.join(run_dir, "dump_" + str(core) + ".bin")))
    return sent_packets, serial_order, dumps


programs = {"sequential": args.sequential,
            "sharing": args.sharing,
            "sharding": args.sharding,
            "scr": args.scr}
binaries = {}
for strategy in programs:
    if programs[strategy] is not None:
        binaries[strategy] = build(strategy, programs[strategy])

input_file = os.path.join(RESULTS_DIR, "in.pcap")
subprocess.run([sys.executable, args.generator, input_file, "--seed", str(args.seed)], check=True)
input_packets = read_pcap_file(input_file)
num_packets = len(input_packets)

results = []


#None means the strategy has no checker yet
def check_strategy(strategy, packets, dumps, ref_packets, ref_dump):
    if strategy == "sharing":
        return check_pcap_equality(packets, ref_packets) and check_sharing(dumps, ref_dump)
    return None


#each strategy on 1 core must behave exactly like the sequential program
if "sequential" in binaries:
    seq_packets, seq_serial, seq_dumps = run_harness(binaries["sequential"], input_file, "sequential", 1, num_packets)
    for strategy in binaries:
        if strategy == "sequential":
            continue
        packets, serial_order, dumps = run_harness(binaries[strategy], input_file, strategy + "_1core", 1, num_packets)
        results.append(("sequential vs " + strategy + " on 1 core", check_strategy(strategy, packets, dumps, seq_packets, seq_dumps[0])))

#an N core run must match a 1 core run that processes the packets in the N core run's commit order
#check B: an N core run must match a 1 core run that processes the packets in the N core run's commit order
if "sharing" in binaries:
    for cores in range(2, args.max_cores + 1):
        run_name = "sharing_" + str(cores) + "cores"
        packets, serial_order, dumps = run_harness(binaries["sharing"], input_file, run_name, cores, num_packets)

        #serial.csv only has packets that reached commit, the rest are added at the end so that
        #the reference still processes them, otherwise a packet wrongly dropped early would go unnoticed
        order = list(serial_order)
        for ts in input_packets:
            if ts not in serial_order:
                order.append(ts)
        ref_input_file = os.path.join(RESULTS_DIR, run_name, "ref_in.pcap")
        write_pcap_file_in_order(ref_input_file, order, input_packets)

        ref_packets, ref_serial, ref_dumps = run_harness(binaries["sharing"], ref_input_file, run_name + "_ref", 1, num_packets)
        passed = ref_serial == serial_order and check_strategy("sharing", packets, dumps, ref_packets, ref_dumps[0])
        results.append(("sharing on 1 core vs " + str(cores) + " cores", passed))

all_passed = True
for (name, passed) in results:
    if passed is None:
        print("SKIP " + name + " (no checker yet)")
    elif passed:
        print("PASS " + name)
    else:
        print("FAIL " + name)
        all_passed = False

if all_passed:
    print("ALL PASS")
else:
    sys.exit(1)
