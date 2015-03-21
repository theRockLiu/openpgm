OpenPGM eases distributed system development in these ways:

### Decoupling and Data Independence ###
Distributed systems can be difficult.  With a large number of hosts all requiring fault-tolerance, load sharing and shared resources an amount of effort is required to develop the capability of such settings, and then the actual configuration and maintenance in the production environment.  OpenPGM allows a simplified communication fabric allowing a distributed plug-and-play component system reducing time and resources for development and operation.

There is no constraint on the data content, this can permit a wide range of environments such as XML transport with XPATH based addressing, JSON for smaller compact independence, or raw data blobs for ultra-high performance.

### Location Transparency ###
OpenPGM can use subject-based addressing, XPATH filtering, or any other technology to direct messages to their destinations, so program processes can communicate without knowing the details of network addresses or connections.

The locations of component processes become entirely transparent; any application component can run on any network host without modification, recompilation or reconfiguration. Application programs migrate easily among host computers. You can dynamically add, remove and modify components of a distributed system without affecting other components.

### Architectural Emphasis on Information Sources and Destinations ###
Decoupling distributed components eliminates much of the complexity commonly associated with network programming.  Systems can be divided by business-logic boundaries rather than technical.

By decomposing a system into sources and destinations of information a more natural, efficient and flexible solution can often be found.

### Reliable Delivery of Whole Messages ###
OpenPGM provides reliable communications between programs, while hiding the burdensome details of network communication and packet transfer from the programmer. The PGM protocol takes care of segmenting and recombining large messages, acknowledging packet receipt, retransmitting lost packets, and arranging packets in the correct order. You can concentrate on whole messages, rather than packets.

While some conventional network APIs guarantee reliable delivery of point‑to‑point messages, most do not guarantee reliable receipt of multicast (or broadcast) messages. Multicast messages can often be lost when some of the intended recipients experience transient network failures.  The PGM protocol offers an open standard method of reliable multicast delivery in which PGM aware routers can also assist in the recovery and consolidation of packet re-requests.
