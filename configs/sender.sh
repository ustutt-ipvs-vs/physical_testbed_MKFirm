# add pcp - skb mapping
sudo ip link add link enp8s0 name enp8s0.100 type vlan id 100 \
	ingress-qos-map 0:0 1:1 2:2 3:3 4:4 5:5 6:6 7:7 \
	egress-qos-map 0:0 1:1 2:2 3:3 4:4 5:5 6:6 7:7 

# Enable all interfaces
sudo ip link set dev enp8s0 up	
sudo ip link set dev enp8s0.100 up

# add IP
sudo ip addr add 192.168.170.1/24 dev enp8s0.100

# add qdiscs
sudo tc qdisc replace dev enp8s0 parent root handle 6666 mqprio num_tc 4 map 3 3 3 3 3 1 0 3 3 3 3 3 3 3 3 queues 1@0 1@1 1@2 1@3 hw 0
sudo tc qdisc replace dev enp8s0 parent 6666:1 etf clockid CLOCK_TAI delta 300000 offload skip_sock_check
sudo tc qdisc replace dev enp8s0 parent 6666:2 etf clockid CLOCK_TAI delta 300000 offload skip_sock_check
