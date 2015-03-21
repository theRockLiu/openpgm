### Introduction ###
The software mechanism for sending and delivering messages is called a transport. A transport defines the delivery scope—that is, the set of possible destinations for the messages it sends.


### Network Transport Parameters ###
Transport creation calls accept three parameters that govern the behaviour of the transport: data-destination port, receive network, and send network.  In simple networking environments one multicast group is sufficient for sending and receiving, however other environments may require further configuration.

  * Several independent distributed applications run on the same network, and you must isolate them from one another (data-destination port).
  * A program runs on a computer with more than one network interface, and you must select a specific network for outbound multicast communications (network parameters).
  * Computers on the network use asymmetric multicast addressing to achieve even higher efficiency, and you must specify which multicast groups to join (network parameters).


### Data-Destination Port Parameter ###
Data-destination ports divide the network into logical partitions. Each transport communicates on a pair of data-source and data-destination ports; a transport can communicate only with other transports on the same destination port. To communicate on more than one port, a program must create a separate transport object for each.


#### Specifying the Data-Destination Port ####
A port number is a decimal integer following the same restrictions as IP sockets with a range 1-65535.


### Data-Source Port Parameter ###
Data-sources ports are the receiving side of PGM and needs to be unique for each application on a host.  By default this is a randomly generated number, 1-65535, however it can be manually specified if you want to deliberately re-join previous sessions or ensure no-overlap between multiple applications on one host.


### Network Parameters ###
PGM is based upon multicast communications, a host can receive multicast traffic on multiple interfaces but can only route outgoing packets through one interface at a time.  The OpenPGM transport accepts a list of receiver interfaces & multicast address groups, and one sender interface & multicast address pair.

To simplify end-user configuration a helper routine can be used to parse TIBCO Rendezvous style network parameter definitions into OpenPGM compatible format with some extensions listed below.

The default network is the administratively scoped multicast address 239.192.0.1 for IPv4 and ff08::1 for IPv6.

#### Constructing the Network Parameter ####
The TIBCO Rendezvous network parameter consists of up to three parts, separated by semicolons—network, multicast groups, send address—as in these examples:
<pre>
lan0                                 network only<br>
lan0;224.1.1.1                       one multicast group<br>
lan0;224.1.1.1,224.1.1.5;224.1.1.6   two multicast groups, send address<br>
</pre>

##### Part One—Interface #####
Part one identifies the network interface, which you can specify a list of interfaces in several ways.  For compatibility with URL addressing square brackets around IPv6 literal addresses will be ignored.  The format of IPv6 scope identifiers depends upon the operating system, typically Windows will use a numeric index, e.g. <tt>fe80::5efe:169.254.203.133%16</tt>, and Unixes will use the adapter name, e.g. <tt>fe80::21e:52ff:fef2:7854%en0</tt>.
<pre>
Interface name§           When an application specifies an interface name, the transport creation<br>
function searches the interface table for the specified interface name.<br>
For example, eth0, eri0, etc.<br>
<br>
Host IP address           A standard IPv4 or IPv6 multi-part address.<br>
For example: "4.2.2.2", "abcd::1", "[b00c::2]"<br>
<br>
Network IP                An IPv4 or IPv6 multi-part network prefix with optional CIDR block size<br>
number                    and scope identifier.<br>
For example: "10.4", "127.1/8", "beef::", "fe80::/64%eth0"<br>
<br>
Host name⁂                A NSS resolvable hosts name.<br>
For example: "localhost"<br>
<br>
Network name†‡¶           A NSS resolvable networks name.<br>
For example: "link-local"<br>
<br>
Default                   The default interface as per multicast routing tables.<br>
<br>
Notes:<br>
§ Specifying an interface name in a multi-scoped IPv6 environment will return an error, the IP<br>
or network address should be specified instead.<br>
<br>
⁂ A default Linux DHCP install will have the hostname resolving to 127.0.1.1 and will broadcast<br>
this address for repairs in effect disabling PGM reliability.  Ensure that the hostname<br>
resolves to an Ethernet link or specify the Ethernet network as the first entity.<br>
<br>
† Linux does not support IPv6 NSS network names and any query will return INADDR_BROADCAST.<br>
<br>
‡ Linux NSS networks only supports class A, B, C networking.<br>
<br>
¶ A compatibility implementation is provided for platforms without full support, see the<br>
section "_NSS Networks Compatibility_".<br>
</pre>


##### Part Two—Receive Groups #####
A list of one or more multicast groups to join, specified as IP addresses or DNS resolvable names, separated by commas. Each address in part two must denote a valid multicast address. Joining a multicast group enables listeners on the resulting transport to receive data sent to that multicast group.

DNS or NSS can be used to specify multicast addresses, some names are pre-defined by IANA such as <tt>ALL-SYSTEMS.MCAST.NET</tt> resolving to 224.0.0.1.

When using Source-Specific Multicast (SSM) through a router IPv4 addresses must be in the range 232/8, IPv6 must be FF3 _x_::/96.  On a simple switched environment any multicast address **may** work but is not guaranteed.

On Linux incoming multicast packets are multiplexed to all listeners regardless of the group joined on the socket.  For example when _application A_ joins groups 239.192.0.1 and 239.192.0.2, whilst _application B_ joins only 239.192.0.1, both will receive packets sent to 239.192.0.2.  This is different behaviour to WinSock on Windows.


##### Part Three-Send Group #####
Part three is a single send address. When a program sends multicast data on the resulting transport, it is sent to this address. The send address need not be among the list of multicast groups joined in part two.

If you join one or more multicast groups in part two, but do not specify a send address in part three, the send address defaults to the first multicast group listed in part two.


#### NSS Networks Compatibility ####
On POSIX platforms network names are resolved through the [Name Service Switch](http://en.wikipedia.org/wiki/Name_Service_Switch) function which allows administrators to deploy configuration through files, LDAP, NIS, and other services.

Unfortunately the Linux networks name service does not support IPv6, an alternative system is provided to read the <tt>/etc/networks</tt> file database directly and extends compatibility to Microsoft Windows patforms that do not export an API at all.

Define the environment variable <tt>PGM_NETDB</tt> with the file database location to override the default system implementation.  On Windows platforms the default configuration file can be found at <tt>%SystemRoot%\system32\drivers\etc\networks</tt>, environmental variables will be expanded as required.


### PGM Sessions ###
When configuring multiple PGM sessions one needs to select appropriate multicast group addresses and PGM ports.  The data-destination port (dport) and multicast group needs to be the same for every host to join one particular session, the data-source port (sport) needs to be only unique on per GSI, i.e. to the host.  For example the following parameters are all unique,

  * Session 1: network ";239.192.0.1", sport 0, dport 7500
  * Session 2: network ";239.192.0.2", sport 0, dport 7500
  * Session 3: network ";239.192.0.2", sport 0, dport 7501
  * Session 4: network ";239.192.0.2", sport 0, dport 7502

Note that multicast filtering on the network occurs at the multicast group address level, port level filtering occurs at the operating system level.


### UDP Encapsulation ###
When using UDP encapsulation many different ports have to be specified and this can get confusing, there is one set of ports for the UDP layer in addition to the set of ports for PGM.

<img src='http://miru.hk/wiki/Udp_encapsulated_ports.png' />


If implementing a system with multiple PGM channels it might be easier to consider fixing the same PGM parameters and altering the UDP parameters for each session, for example:

  * Session 1: network ";239.192.0.1", sport 0, dport 7500, udp-port 3055.
  * Session 2: network ";239.192.0.2", sport 0, dport 7500, udp-port 3056.

With a small number of transports one should choose a unique multicast group address and UDP-port for each.  Multicast filtering occurs at the address level such that all packets for any port will be passed to the operating system which then performs the port filtering.  All benefit of network filtering be lost if only using multiple ports on the same multicast group address.

Be aware that on Linux it is the node that joins multicast groups, viewable via <tt>netstat -g</tt>, incoming packets are multiplexed to all matching port listeners.  This means it is insufficient to simply change the multicast address for different data streams unless the node is guaranteed to only join one stream at any time.

Multiple applications on one host need to enable multicast loop for port sharing, example settings:

  * Application A: network ";239.192.0.1", sport 0, dport 7500, udp-port 3055.
  * Application B: network ";239.192.0.1", sport 0, dport 7500, udp-port 3055.

Note that Windows and POSIX differ on which socket side needs to enable multicast loop, it is therefore recommended for portability to set for both publisher and subscriber.