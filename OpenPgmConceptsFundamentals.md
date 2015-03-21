### Multicast and Point-to-Point Messages ###
OpenPGM is a reliable multicast communication, point-to-point delivery is not within the scope of PGM.


### Reliable Messaging ###
First let's define what is meant by reliable messaging.  Consider two parties in a communication, one sending information the other receiving.

<img src='http://miru.hk/wiki/PGM_sender_to_receiver.png' />

This could be a telephone call with only one person talking.  We update the diagram indicating that the conversation comprises a series of information bites.

<img src='http://miru.hk/wiki/PGM_sender_to_receiver_sequence.png' />

Each piece of information could be a sentence or a word during a telephone call, the pieces are numbered in the diagram to indicate flow from left to right, from the sender to the receiver.  Now consider that one part of the conversation was lost.

<img src='http://miru.hk/wiki/PGM_sender_to_receiver_loss.png' />

During a telephone call maybe you hear some static noise, if using a mobile phone the signal is lost and you here nothing for part of the conversation.  When the receiver here's the conversation continue again they notice something was missing and might ask the sender to repeat what they said.

<img src='http://miru.hk/wiki/PGM_sender_to_receiver_repeat.png' />

The sender can repeat what was previously sent and the communication can continue as before.

What happens if the sender was using a mobile phone and the battery is flat?  The sender never receives the request to repeat and they can no longer continue the conversation.


### Multicast Messages ###
A multicast message is any message with many potential recipients.  This contrasts to unicast with one recipient, and broadcast to all available recipients.

<img src='http://miru.hk/wiki/PGM_casting.png' />

These potential recipients are called subscribers.  The term subscriber is used as the network sends out requests to join the multicast group.  Actual routing of multicast group interest is usually more optimal than the diagram implies.

<img src='http://miru.hk/wiki/PGM_multicast_elements.png' />

Every subscriber receives the same message.  The set of subscribers can dynamically change, however even if no subscribers are active the message is still transmitted on the wire.

If desired, a subscription layer could be implemented that defines a mechanism such as a subject or content filter that can further restrict delivery of that message to the application layer.

<img src='http://miru.hk/wiki/OpenPGM_multicast_message.png' />

Non-subscribed PGM streams are filtered out by a network switch if the publisher is on the same segment, if the switch is overloaded filtering can happen at the interface card level, and when that is overloaded the task is performed by the operating system.


### Publish/Subscribe Interactions ###
Publish/subscribe interactions are driven by events, communication is in one-way, and often one-to-many. The complete interaction consists of one multicast message, published once, and received by all subscribers.

<img src='http://miru.hk/wiki/OpenPGM_publish_subscribe.png' />

Example applications:

  * Stock price data publishing to thousands of traders.
  * Inventory levels to accounting, purchasing, and marketing in a retail chain.

In publish/subscribe interactions, data producers are decoupled from data consumers—they do not coordinate data transmission with each other. Producers publish data to the network at large.
Consumers place a standing request for data by subscribing.


### Multicast Request Reply Interactions ###
While traditional request/reply interactions involve one requester and one server, in multicast request/reply interactions multiple servers can receive the request and respond as appropriate.  The subsequent communication is usually point-to-point and this could be over a more suitable protocol such as TCP.

<img src='http://miru.hk/wiki/OpenPGM_multicast_request_reply.png' />

Example applications:

  * Database query with multiple servers.
  * Load sharing computation cluster, the first server receiving the request processes the information.
  * Network management applications that multicast requests for the same information from multiple hosts.


### Segmentation ###
TCP sockets provide a continuous channel of information between the sender and receiver, PGM like UDP sockets however implement a datagram delivery scheme.  This means the application has to implement a segmentation scheme to split large streams into smaller units that can be individually sent.

<img src='http://miru.hk/wiki/PGM_packet_sizes.png' />

Each packet sent over IP is TPDU bytes in length, consisting of a PGM header and TSDU bytes payload.

PGM has primitive support for larger packets as defined by the application layer, called APDUs.  Each APDU is segmented into multiple PGM packets, and re-assembled before delivery to the receiving application.

<img src='http://miru.hk/wiki/PGM_APDUs.png' />


### Transmission Groups ###
When Reed-Solomon forward error correction (FEC) is enabled the concept of transmission groups is introduced.

<img src='http://miru.hk/wiki/PGM_transmission_group.png' />

A transmission group is a series of _k_ packets as defined by the Reed-Solomon code RS(_n_, _k_).  Packet recovery mechanisms are delayed until the end of a transmission group is reached or discovered.  If pro-active parity is enabled parity packets are broadcast after the original data packets, if on-demand parity is enabled parity packets are sent instead of re-transmitting original data.
