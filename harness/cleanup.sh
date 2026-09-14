#!/bin/bash
if ip link show veth0 &>/dev/null;then
    sudo ip link delete veth0
fi
if ! ip link show veth0 &>/dev/null && ! ip link show veth1 &>/dev/null; then
    echo "Deleted veth pair (veth0,veth1)"
fi