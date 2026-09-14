#!/bin/bash

sudo ip link add veth0 type veth peer name veth1 &>/dev/null

sudo ip link set veth0 up &>/dev/null
sudo ip link set veth1 up &>/dev/null

if ip link show veth0 &>/dev/null && ip link show veth1 &>/dev/null; then
    echo "Created veth pair (veth0,veth1)"
else
    echo "Issue with setup"
fi