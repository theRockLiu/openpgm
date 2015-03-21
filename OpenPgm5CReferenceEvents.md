﻿#summary OpenPGM 5 : C Reference : Events
#labels Phase-Implementation
#sidebar TOC5CReferenceEvents
## Introduction ##
NAK and NCF processing requires that one thread is always processing incoming events.  With a synchronous strategy this should be one dedicated application thread reading messages and copying or processing into an internal queue for on-demand processing.

Event driven programs idle in an event loop dispatching events as they are raised.  OpenPGM application developers can choose whether to follow an event driven model or regular imperative styles.

Events can come from any number of different types of sources such as file descriptors (plain files, pipes or sockets) and timeouts.  With OpenPGM events can be driven from the underlying network transport, timing state engine, or at a higher level from an asynchronous data queue.

When relying on the underlying network transport for event generation the developer can detect level events with POSIX functions <tt>select</tt>, <tt>poll</tt>, or higher speed with edge levels via <tt>epoll</tt>.  The following table provides a summary of the APIs that allow integration with such mechanisms:

<table cellpadding='5' border='1' cellspacing='0'>
<tr>
<th>Synchronisation Strategy</th>
<th>Event Mechanism</th>
<th>OpenPGM API</th>
</tr>
<tr>
<td>Synchronous</td>
<td><tt>select()</tt></td>
<td><tt><a href='OpenPgm5CReferencePgmSelectInfo.md'>pgm_select_info()</a></tt></td>
</tr><tr>
<td><tt>poll()</tt> or <tt>WSAPoll()</tt></td>
<td><tt><a href='OpenPgm5CReferencePgmPollInfo.md'>pgm_poll_info()</a></tt></td>
</tr><tr>
<td><tt>epoll()</tt></td>
<td><tt><a href='OpenPgm5CReferencePgmEpollCtl.md'>pgm_epoll_ctl()</a></tt></td>
</tr>
</table>


Note that the special APIs for synchronous events as multiple file descriptors need to be monitored for correct operation, see the section [Data Triggers](OpenPgmConceptsEvents.md) in _OpenPGM Concepts_ for further details.

## Receiver Events ##
<img src='http://miru.hk/wiki/receiver-events.png' />

PGM receivers receive three types of events, from left-to-right in the diagram:
  1. Timers due to expiration of receive state, this includes sending NAKs and detecting unrecoverable data loss.
  1. Read events from data waiting to be read from the receive window.
  1. Read events from the raw underlying socket driving the PGM protocol, such as SPMs and NCFs.

Note that not all events will cause data to be returned but are necessary to drive the PGM protocol engine.

## Sender Events ##
<img src='http://miru.hk/wiki/sender-events.png' />

PGM senders have more complicated event handling requirements due to reliable delivery.  There are five types of events, from left-to-right in the diagram:
  1. Timer due to transmit rate limit, i.e. how long until there are sufficient tokens to send a new data packet.
  1. Non-blocking writing events for when the system buffers are exhausted, typically when no rate limit is used or it is too high.
  1. Timers due to change in transmit state, namely sending ambient and heartbeat SPMs.
  1. Read events from pending transmission of repair data, transmission of repair data can be delayed due to rate limiting and for NAK elimination in order to avoid NAK implosion.
  1. Read events from the raw underlying socket driving the PGM protocol, such as NAKs and SPMRs.