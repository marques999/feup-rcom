#!/bin/bash

/etc/init.d/networking restart		# reload default configurations
ifconfig eth0 up					# activate interface eth0
ifconfig eth0 172.16.30.254/24		# identify interface eth0 (172.16.30.254)
ifconfig eth1 up					# activate interface eth1
ifconfig eth1 172.16.31.253/24		# identify interface eth1 (172.16.31.253)
route add default gw 172.16.31.254	# add Rc as default gayeway of tuxy2

echo 1 > /proc/sys/net/ipv4/ip_forward	
echo 0 > /proc/sys/net/ipv4/icmp_echo_ignore_broadcasts

# PARTE 5 - DNS
cp /etc/resolv.conf /etc/resolv.conf.backup
echo "search netlab.fe.up.pt" > /etc/resolv.conf
echo "nameserver 172.16.1.1" >> /etc/resolv.conf

iptables -t nat -A POSTROUTING -o eth1 -j MASQUERADE
iptables -A FORWARD -i eth1 -m state --state NEW,INVALID -j DROP
