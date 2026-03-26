sudo tc qdisc delete dev enp8s0 parent root handle 6666

# add qdiscs
sudo tc qdisc replace dev enp8s0 parent root handle 6666 mqprio num_tc 4 map 3 3 3 3 3 1 0 3 3 3 3 3 3 3 3 queues 1@0 1@1 1@2 1@3 hw 0
sudo tc qdisc replace dev enp8s0 parent 6666:1 etf clockid CLOCK_TAI delta 300000 offload skip_sock_check
sudo tc qdisc replace dev enp8s0 parent 6666:2 etf clockid CLOCK_TAI delta 300000 offload skip_sock_check
