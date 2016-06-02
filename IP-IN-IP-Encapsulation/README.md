#Efficient IP Encapsulation within IP

##Procedures

All packets that Gatekeeper decides to forward are encapsulated using IP-in-IP protocol, and fills the new IP header as follows:
```
(1) source IP address is the IP address of the server.

(2) destination IP address is a secondary IP address of the destination derived according to the configuration of the Gatekeeperd.

(3) the DSCP field is the number 2.
```
##Unit Test
This repository currently supports IP checksum offloading, parsing IPv4/6 headers, etc. The code has been tested by using the [DPDK Ubuntu 14.04 Vagrant VM](https://github.com/cjdoucette/ubuntu-dpdk), and [Ostinato](http://ostinato.org/) to generate packets. The test procedure is illustrated as the following four steps:

```
(1) Mac vboxnet0 (ether 0a:00:27:00:00:00) ----> generate/send IP-in-IP packets (capture packets on vboxnet0 using wireshark) ----> 

(2) DPDK port 0 (ether 08:00:27:d2:57:54) ----> receive packets (dump original packets) ----> decapsulation() (dump decapsulated packets) ----> encapsulation() (dump encapsulated packets)---->

(3) Forward the packets to DPDK port 1 (ether 08:00:27:7d:c7:68) ----> send new IP-in-IP packets ----> 

(4) Mac vboxnet1 (ether 0a:00:27:00:00:01) ----> receive packets from DPDK port 1 (capture packets on vboxnet1 using wireshark)
```
