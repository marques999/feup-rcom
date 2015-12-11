#!/bin/bash

/etc/init.d/networking restart		# reload default configurations
ifconfig eth0 up					# activate eth0
ifconfig eth0 172.16.31.1/24		# identify eth0

route add -net 172.16.30.0/24 gw 172.16.31.253		# add route to tuxy4 in order to access network 172.16.30.0/24
route add default gw 172.16.31.254					# add Rc as default gayeway of tuxy2

echo 1 > /proc/sys/net/ipv4/ip_forward
echo 0 > /proc/sys/net/ipv4/icmp_echo_ignore_broadcasts

#PARTE 5 - DNS
cp /etc/resolv.conf /etc/resolv.conf.backup
echo "search netlab.fe.up.pt" > /etc/resolv.conf
echo "nameserver 172.16.1.1" >> /etc/resolv.conf
