### Introduction ###
The state of a transport is bounded by PGM sessions between a sender and one or more receivers.  The scope of session can be explicitly defined by PGM options or auto-discovered at runtime.


### Session Start ###
The presence of a PGM sender is noted when the first packet with a new TSI is received.  This may be a Source-Path Message (SPM) simply defining the distribution tree or a data packet indicating late joining.


#### SPM Distribution Tree ####
A PGM distribution tree is defined by the flow of Source Path Messages (SPMs) from a source through network elements to subscribers.  PGM network elements use the SPM information to address returning unicast NAKs directly to the next upstream PGM network toward the source, and thereby insure that NAKs return from a receiver to a source on the reverse of the distribution path for any TSI.

<img src='http://miru.hk/wiki/SPM_distribution_tree.png' />

A receiving application joining a PGM multicast address will see the existence of a PGM session by its regular SPM broadcast.  If the sender has already transmitted data packets it is considered late joining.


#### Late Joining ####
A new receiving application joining a PGM session already in flow is called late joining.

<img src='http://miru.hk/wiki/Late_joining.png' />

The PGM specification defines that the receive window is bound by the first sequence number received.  So in the example RXW\_TRAIL\_INIT is set to 3, that means the receiver is not permitted to request packets with an earlier sequence number.  This can be overridden with the OPT\_JOIN - Late Joining option.


#### OPT\_SYN ####
A PGM session can be created and left idling with only SPM broadcasts.  The OPT\_SYN option can be used to indicate that a particular data packet defines the actual start of the data session.  For example this could be the start of a video broadcast.


#### OPT\_RST ####
Dependent upon the application domain an OPT\_RST option defines that a data session has been reset.  For example if a satellite video feed was being re-broadcast over PGM and was interrupted the OPT\_RST option could be used to indicate an satellite connection failure occurred and will shortly re-start with a clean state.


### Session Termination ###
The active list of sessions is maintained as state information of a PGM receiver.  Two methods are available for marking sessions as no longer valid, freeing up held resources.

#### OPT\_FIN ####
Dependent upon the application domain an OPT\_FIN indicates that a source has finished its session.  For example this could indicate the end of a video broadcast.

<img src='http://miru.hk/wiki/SYN_and_FIN.png' />

In order to ensure delivery of session termination the sending transport continues to broadcast SPMs with the OPT\_FIN option until the transport is terminated.


#### Peer Expiry ####
Delivery of OPT\_FIN is still not guaranteed.  A video broadcast application might stop sending data and indicate session termination with the OPT\_FIN option, however if the application is closed the transport will be terminated stopping future publishes of SPM packets.  Subscribing applications to a terminated transport will no longer see any packets.  In some application domains, such as television, the subscribing application could have a permanent association between a senders TSI and a broadcast channel, therefore even if the senders transport terminated it is expected to sometime later be restarted.  In other application domains sessions are only temporary, to be discarded when the transport is closed.  If a receiving application did not receive closure notification it should assume after a period of no deliveries that the sender has terminated.

In rare circumstances it is of course possible for a session to be terminated by a period of inactivity, but then resumed later due to intermediary network failures.  The new data will start a new session with any intermediate packets unrecoverable due to late joining restrictions.  It is important for the application designer to be aware of the difference between persistent and temporary transport sessions.
