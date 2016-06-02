# Gatekeeper
This project contains several components:

## Directing Incoming Packets
Fast network interfaces such as 10Gbps and 40Gbps generate more packets than a single lcore can consume. Therefore, we need to load balance these packets through a number of lcores to reach line speed.

## Efficient IP Encapsulation within IP
All packets that Gatekeeper decides to forward are encapsulated using IP-in-IP protocol
