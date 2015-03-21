﻿#summary OpenPGM : C Reference : pgm\_transport\_create()
#labels Phase-Implementation

_Function_
### Declaration ###
<pre>
#include <pgm/pgm.h><br>
<br>
int *pgm_transport_create* (<br>
[OpenPgmCReferencePgmTransportT pgm_transport_t]* const           transport,<br>
[OpenPgmCReferencePgmGsiT pgm_gsi_t]*                       gsi,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#guint16 guint16]                          sport,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#guint16 guint16]                          dport,<br>
struct [http://www.ietf.org/rfc/rfc3678.txt group_source_req]*         recv_gsr,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gsize gsize]                            recv_len,<br>
struct [http://www.ietf.org/rfc/rfc3678.txt group_source_req]*         send_gsr<br>
);<br>
</pre>

### Purpose ###
Create a network transport.

### Remarks ###
A custom network protocol requires super-user privileges to open the necessary raw sockets.  An application can call <tt>pgm_transport_create()</tt> to create unbound raw sockets, drop the privileges to maintain security, and finally bind with <tt><a href='OpenPgmCReferencePgmTransportBind.md'>pgm_transport_bind()</a></tt> to start processing incoming packets.

A PGM data-source port is a unique inbox per GSI different from the data-destination port.  When an application restarts it should use a new data-source port to distinguish its sequence number usage from previous instances.  Special applications such as file carousels may wish to re-join previous sessions if sequence numbers are mapped to file contents.  Specifying a value of 0 for a PGM data-source port will randomly generate a unique value.  Administrators running multiple PGM applications on the same host using multicast loop should pay attention to the possibility that serious communication problems will occur if applications pick the same data-source port.

Both <tt>recv_gsr</tt> and <tt>send_gsr</tt> are populated as per RFC 3678.  <tt><a href='OpenPgmCReferencePgmIfParseTransport.md'>pgm_if_parse_transport()</a></tt> provides a convenient method of defining the network parameters with a TIBCO Rendezvous network parameter compatible string.  By default an any-source multicast configuration is enabled, set <tt>gsr_source</tt> to the IP address of a sending computer for source-specific multicast (SSM).  Set <tt>sin_port</tt> of <tt>send_gsr</tt> and all <tt>recv_gsr</tt> entities to specify the port to use for UDP encapsulation.

SSM destination addresses must be in the ranges 232/8 for IPv4 or FF3 _x_::/96 for IPv6.

Try to avoid 224.0.0.x addresses as many of these [multicast addresses](http://en.wikipedia.org/wiki/Multicast_address) are used by network infrastructure equipment and have special purposes.  The recommendation on private networks is to used the administratively scoped range 239/8 (239.192/16 for local-scope) as per RFC 2365 and RFC 3171.  Public Internet usage of multicast should be careful of conflicts with [IANA assigned multicast addresses](http://www.iana.org/assignments/multicast-addresses) and should consider 233/8 [GLOP addressing](http://en.wikipedia.org/wiki/GLOP) as per RFC 3180.

IP Multicasting uses Layer 2 MAC addressing to transmit a packet to a group of hosts on a LAN segment.  The Layer 3 address (224.0.0.0 to 239.255.255.255) are mapped to Layer 2 addresses (0x0100.5E _xx_._xxxx_).  Because all 28 bits of the Layer 3 IP multicast address information cannot be mapped into the available 23 bits of MAC address space, five bits of address information are lost in the mapping process.  This means that each IP multicast MAC address can represent 32 IP multicast addresses.

IGMP Snooping normally is used by Layer 2 switches to constrain multicast traffic only to those ports that have hosts attached and that have signalled their desire to join the multicast group by sending IGMP Membership Reports.  IGMP Snooping is disabled for the special 224.0.0.1 - <tt>ALL-HOSTS</tt> group, 224.0.0.2 - <tt>ALL-ROUTERS</tt> group, 224.0.0.5 - <tt>ALL-OSPF-ROUTERS</tt> group, etc.  This means any IP multicast group matching 224-239.0.0/24 and 224-239.128.0/24 will also cause Layer 2 flooding and should be avoided.

### Parameters ###

<table cellpadding='5' border='1' cellspacing='0'>
<tr>
<th>Parameter</th>
<th>Description</th>
</tr>
<tr>
<td><tt>transport</tt></td>
<td>The PGM transport object.</td>
</tr><tr>
<td><tt>gsi</tt></td>
<td>A Globally unique source identifier (GSI).</td>
</tr><tr>
<td><tt>sport</tt></td>
<td>Data-source port, specify a value of 0 to randomly assign a value.</td>
</tr><tr>
<td><tt>dport</tt></td>
<td>Data-destination port.</td>
</tr><tr>
<td><tt>recv_gsr</tt></td>
<td>Receive interface, multicast group, and source address.</td>
</tr><tr>
<td><tt>recv_len</tt></td>
<td>The number of interface & group pairs stored in <tt>recv_smr</tt>.</td>
</tr><tr>
<td><tt>send_gsr</tt></td>
<td>Sending interface and multicast group.  Source address is ignored, so effectively only a <a href='http://www.ietf.org/rfc/rfc3678.txt'>group_req</a> structure.</td>
</tr>
</table>


### Return Value ###
On success, returns 0.  On invalid arguments, returns <tt>-EINVAL</tt>.  On error, returns -1 and sets <tt>errno</tt> appropriately.

### Errors ###
<dl><dt><tt>EAGAIN</tt></dt><dd>The system lacked the necessary resources to create another thread, or the system-imposed limit on the total number of threads in a process <tt>PTHREAD_THREADS_MAX</tt> would be exceeded.<br>
</dd><dt><tt>EACCES</tt></dt><dd>Permission to create a socket of the specified type is denied.<br>
</dd><dt><tt>EAFNOSUPPORT</tt></dt><dd>The implementation does not support the specified address family.<br>
</dd><dt><tt>EINVAL</tt></dt><dd>Unknown protocol, or protocol family not available.<br>
</dd><dt><tt>EMFILE</tt></dt><dd>Too many file descriptors are in use by the process.<br>
</dd><dt><tt>ENFILE</tt></dt><dd>The system limit on the total number of open files has been reached.<br>
</dd><dt><tt>ENETDOWN</tt></dt><dd>Interface is not up.<br>
</dd><dt><tt>ENOBUFS</tt> or <tt>ENOMEM</tt></dt><dd>Insufficient memory is  available.   The  socket  cannot  be  created  until  sufficient resources are freed.<br>
</dd><dt><tt>EPERM</tt></dt><dd>User has insufficient privileges to carry out this operation.<br>
</dd><dt><tt>EPROTONOSUPPORT</tt></dt><dd>The protocol type or the specified protocol is not supported within this domain.<br>
</dd></dl>

### Example ###
Regular PGM protocol example using the <tt>eth0</tt> interface (popular Linux adapter name), the multicast address 239.192.0.1, and the data-destination port 7500.

```
 pgm_gsi_t gsi;
 guint16 dport = 7500;
 char* network = "eth0;239.192.0.1";
 struct group_source_req recv_gsr, send_gsr;
 gsize gsr_len = 1;
 pgm_transport_t* transport;
 
 pgm_create_md5_gsi (&gsi);
 pgm_if_parse_transport (network, AF_INET, &recv_gsr, &send_gsr, &gsr_len);
 pgm_transport_create (&transport, &gsi, 0, dport, &recv_gsr, gsr_len, &send_gsr);
```

A UDP encapsulated example using UDP port 3055.

```
 pgm_gsi_t gsi;
 guint16 dport = 7500;
 guint16 udp_encap_port = 3055
 char* network = "eth0;239.192.0.1";
 struct group_source_req recv_gsr, send_gsr;
 gsize gsr_len = 1;
 pgm_transport_t* transport;
 
 pgm_create_md5_gsi (&gsi);
 pgm_if_parse_transport (network, AF_INET, &recv_gsr, &send_gsr, &gsr_len);
 ((struct sockaddr_in*)&send_gsr.gsr_group)->sin_port = g_htons (udp_encap_port);
 ((struct sockaddr_in*)&recv_gsr.gsr_group)->sin_port = g_htons (udp_encap_port);
 pgm_transport_create (&transport, &gsi, 0, dport, &recv_gsr, gsr_len, &send_gsr);
```

A source-specific multicast session, subscribing to group 232.0.1.1 from host 172.16.0.1.

```
 pgm_gsi_t gsi;
 guint16 dport = 7500;
 char* network = ";232.0.1.1";
 char* source  = "172.16.0.1";
 struct group_source_req recv_gsr, send_gsr;
 gsize gsr_len = 1;
 pgm_transport_t* transport;
 
 pgm_create_md5_gsi (&gsi);
 pgm_if_parse_transport (network, AF_INET, &recv_gsr, &send_gsr, &gsr_len);
 ((struct sockaddr_in*)&recv_gsr.gsr_source)->sin_addr.s_addr = inet_addr(source);
 pgm_transport_create (&transport, &gsi, 0, dport, &recv_gsr, gsr_len, &send_gsr);
```

Manually populate the GSR receive and send entities with ASM group 239.192.0.2.

```
 pgm_gsi_t gsi;
 guint16 dport = 7500;
 char* network = ";232.0.1.1";
 struct group_source_req recv_gsr, send_gsr;
 pgm_transport_t* transport;
 
 pgm_create_md5_gsi (&gsi);
 memset (&recv_gsr, 0, sizeof(recv_gsr));
 ((struct sockaddr_in*)&recv_gsr.gsr_group)->sin_family = AF_INET;
 ((struct sockaddr_in*)&recv_gsr.gsr_group)->sin_addr.s_addr = inet_addr(network);
 memcpy (&recv_gsr.gsr_source, &recv_gsr.gsr_group, sizeof(recv_gsr.gsr_group));
 memcpy (&send_gsr, &recv_gsr, sizeof(recv_gsr));
 pgm_transport_create (&transport, &gsi, 0, dport, &recv_gsr, 1, &send_gsr);
```

### See Also ###
  * <tt><a href='OpenPgmCReferencePgmTransportT.md'>pgm_transport_t</a></tt><br>
<ul><li><tt><a href='OpenPgmCReferencePgmIfParseTransport.md'>pgm_if_parse_transport()</a></tt><br>
</li><li><tt><a href='OpenPgmCReferencePgmTransportBind.md'>pgm_transport_bind()</a></tt><br>
</li><li><a href='OpenPgmCReferenceTransport.md'>Transport</a> in OpenPGM C Reference.<br>
</li><li><tt>struct <a href='http://www.ietf.org/rfc/rfc3678.txt'>group_source_req</a></tt> in RFC 3678.<br>
</li><li><a href='http://www.iana.org/assignments/multicast-addresses'>Assigned multicast addresses</a> at <a href='http://en.wikipedia.org/wiki/Internet_Assigned_Numbers_Authority'>IANA</a>.<br>
</li><li><a href='http://www.cisco.com/en/US/tech/tk828/technologies_white_paper09186a00802d4643.shtml'>Guidelines for Enterprise IP Multicast Address Allocation</a> at Cisco.<br>
</li><li><a href='http://en.wikipedia.org/wiki/Multicast_address'>Multicast address</a> in Wikipedia.<br>
</li><li><a href='http://en.wikipedia.org/wiki/Any-source_multicast'>Any-source multicast (ASM)</a> in Wikipedia.<br>
</li><li><a href='http://en.wikipedia.org/wiki/Source-specific_multicast'>Source-source multicast (SSM)</a> in Wikipedia.<br>
</li><li><a href='http://en.wikipedia.org/wiki/Internet_Group_Management_Protocol'>Internet Group Management Protocol (IGMP)</a> in Wikipedia.<br>
</li><li><a href='http://en.wikipedia.org/wiki/Multicast_Listener_Discovery'>Multicast Listener Discovery (MLD)</a> in Wikipedia.