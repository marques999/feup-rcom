#!/bin/bash

/etc/init.d/networking restart		# reload default configurations
ifconfig eth0 up					# activate interface eth0
ifconfig eth0 172.16.30.1/24		# identify interface eth0 (172.16.30.1)

route add -net 172.16.31.0/24 gw 172.16.30.254		# add route to tuxy4 in order to access network 172.16.31.0/24
route add default gw 172.16.30.254					# add tuxy4 as default gateway of tuxy1

echo 1 > /proc/sys/net/ipv4/ip_forward	
echo 0 > /proc/sys/net/ipv4/icmp_echo_ignore_broadcasts

#PARTE 5 - DNS
cp /etc/resolv.conf /etc/resolv.conf.backup
echo "search netlab.fe.up.pt" > /etc/resolv.conf
echo "nameserver 172.16.1.1" >> /etc/resolv.conf
