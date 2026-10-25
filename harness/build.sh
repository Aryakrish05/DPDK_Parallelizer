#!/bin/bash

# GenAI used for generation of this script

# usage: ./build.sh [path/to/program.hpp] [output]
# without a program, builds Program.hpp into ./harness

cd "$(dirname "$0")"

if [ -z "$1" ]; then
    g++ -std=c++20 -O2 -Wall -pthread -I. main.cpp -o harness
    exit $?
fi

program="$(realpath "$1")"
output="${2:-harness_$(basename "$program" .hpp)}"
g++ -std=c++20 -O2 -Wall -pthread -I. -DPROGRAM="\"$program\"" main.cpp -o "$output"
