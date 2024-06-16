#!/bin/bash

# https://bbs.archlinux.org/viewtopic.php?id=207907

if [[ $(whoami) != "root" ]]; then
    echo "$0: you must be root"
    exit 1
fi

if [ $# -ne 1 ]; then
    echo "$0: Usage:"
    echo "    $0 username"
    exit 1
fi

ip link add name br0 type bridge
ip addr add 192.168.8.231/24 dev br0
ip link set br0 up
dnsmasq --interface=br0 --bind-interfaces --dhcp-range=192.168.8.232,192.168.8.239

modprobe tun
ip tuntap add dev tap0 mode tap user "$1"
ip link set tap0 up promisc on
ip link set tap0 master br0

sysctl net.ipv4.ip_forward=1
sysctl net.ipv6.conf.default.forwarding=1
sysctl net.ipv6.conf.all.forwarding=1

sysctl net.ipv6.conf.br0.accept_ra=2
ip6tables -t nat -A POSTROUTING -o wlp0s20f3 -j MASQUERADE
ip6tables -A FORWARD -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
ip6tables -A FORWARD -i tap0 -o wlp0s20f3 -j ACCEPT

ip -family inet6 route add default via fe80::b2cc:feff:fefb:3b2b dev wlp0s20f3

iptables -t nat -A POSTROUTING -o wlp0s20f3 -j MASQUERADE
iptables -A FORWARD -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
iptables -A FORWARD -i tap0 -o wlp0s20f3 -j ACCEPT

