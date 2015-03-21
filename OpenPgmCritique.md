# Introduction #
This section covers the points which might be considered negatives about this PGM implementation. Is could also conversely be considered a collection of goals for future development.


# Complicated Parameters #
OpenPGM does not provide PGM default parameters, this is necessary to introduce that the parameters need tuning to the destination environment.  With very high speed messaging, which means from 100,000 to millions of packets-per-second (pps), it is not practical to have a reliability buffer many seconds in length.  Conversely low data rate messaging, such as system monitoring, it is superfluous trying to ensure microsecond latencies.


# "_Advance with Time_" only #
The transmit window advances with time but with a fixed window size, i.e. <tt>TXW_BYTES</tt> bytes of data.  "_Advance with Data_" as per the specification means new transmissions are halted until no NAKs have been seen for <tt>TXW_ADV_IVL</tt> seconds.


# No Adaptive NAK Implosion #
Large communication environments can see a significant generation of NAKs.  When across multiple networks it can be difficult to fix and determine optimum PGM parameters.  Appendix E of the PGM specification introduces a mechanism of adapting the NAK back-off interval to the current network conditions.


# No Congestion Control #
An extra draft proposal was created for congestion control:  [draft-ietf-rmt-bb-pgmcc-03](http://www3.tools.ietf.org/html/draft-ietf-rmt-bb-pgmcc-03).  This was not incorporated into the main RFC for PGM, for which OpenPGM just covers the basic functionality.
OpenPGM does include rate regulation using a leaky bucket scheme.


# No RDMA Support #
New technologies like 10 GigE and InfiniBand provide user-space transports through the RDMA protocol.  This alleviates the biggest performance problem of system call overhead.

Alternatives to RDMA are more invasive to the operating environment such as multi-packet variants of <tt>sendmsg()</tt> and <tt>recvmsg()</tt>, and simply moving the core PGM logic inside kernel space as part of the core networking set.  Both still need context switches and do not scale well on the faster networking fabrics.


# User-space Protocol Disadvantages #
Not integrated with an operating system kernel gives OpenPGM some advantages and disadvantages.

  * High cost of user-space to kernel context switch per packet.
  * No zero-copy UDP system calls increase switching cost.
  * No hardware acceleration of PGM checksumming.
  * NAK processing requires kernel-to-user-to-kernel switches.


# General PGM Disadvantages #
OpenPGM inherits all the issues fundamental to PGM as a network protocol compared with regular unreliable UDP, or connection orientated TCP.

  * Increased latency over unreliable UDP, from marginal, measured in microseconds to significant for recovered packets.
  * No security for authentication of peers or verification of content.
  * Back-link from subscriber to publisher.
  * High packet loss rates can overwhelm senders re-transmitting repair data.


# General IP Multicast Disadvantages #
OpenPGM inherits all the issues common with IP multicast.

  * High packet rates can overwhelm network interfaces and devices.
  * Very high packet rates can render switches into basic hubs, and interfaces to forward all packets to the operating system significantly increasing systems loads and adversely affecting other traffic.
  * Routing between VLANs on layer 2-3 switches usually requires an external multicast router.
  * Not every router supports IPv4 multicast routing, although Cisco support is very comprehensive and multicast is a mandatory requirement for IPv6.
  * Every router between senders and receivers must support multicast routing.
  * Not every ISP provides MBONE connectivity in order to use multicast over the Internet.
  * Any-source multicast (ASM) highly subject to interference and high load from DoS attacks requiring application and hardware support for Source-specific multicast (SSM) by IGMPv3.