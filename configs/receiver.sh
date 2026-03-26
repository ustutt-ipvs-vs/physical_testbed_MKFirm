# add pcp - skb mapping
sudo ip link add link enp8s0 name enp8s0.100 type vlan id 100 \
	ingress-qos-map 0:0 1:1 2:2 3:3 4:4 5:5 6:6 7:7 \
	egress-qos-map 0:0 1:1 2:2 3:3 4:4 5:5 6:6 7:7 

# Enable all interfaces
sudo ip link set dev enp8s0 up	
sudo ip link set dev enp8s0.100 up

# add IP
sudo ip addr add 192.168.170.2/24 dev enp8s0.100
