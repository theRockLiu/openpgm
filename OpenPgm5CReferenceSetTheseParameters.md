﻿#summary OpenPGM 5 C Reference : Set These Parameters
#labels Phase-Implementation
#sidebar TOC5CReferenceProgrammersChecklist
OpenPGM does not default with a set of PGM parameters as it is necessary to customize to your messaging platform requirements. OpenPGM applications need to include calls to the following functions before connecting a PGM socket with a call to <tt><a href='OpenPgm5CReferencePgmConnect.md'>pgm_connect()</a></tt>


<table cellpadding='5' border='1' cellspacing='0'>
<tr>
<th>Socket Option</th>
<th>Description</th>
</tr>
<tr>
<td><h3>Sending</h3></td>
</tr><tr>
<td><tt>PGM_MTU</tt></td>
<td>Set maximum transport data unit size.</td>
</tr><tr>
<td><tt>PGM_TXW_SQNS</tt>, or<br />
<tt><a href='OpenPgmConceptsTxwMaxRte.md'>PGM_TXW_MAX_RTE</a></tt> and <tt>PGM_TXW_SECS</tt></td>
<td>Set send window size, optionally enabling rate regulation engine.</td>
</tr><tr>
<td><tt>PGM_MULTICAST_HOPS</tt></td>
<td>Set maximum number of network hops to cross.</td>
</tr><tr>
<td><tt>PGM_AMBIENT_SPM</tt></td>
<td>Set interval of background SPM packets.</td>
</tr><tr>
<td><tt>PGM_HEARTBEAT_SPM</tt></td>
<td>Set intervals of data flushing SPM packets.</td>
</tr><tr>
<td><h3>Receiving</h3></td>
</tr><tr>
<td><tt>PGM_MTU</tt></td>
<td>Set maximum transport data unit size.</td>
</tr><tr>
<td><tt>PGM_RXW_SQNS</tt>, or<br />
<tt>PGM_RXW_MAX_RTE</tt> and <tt>PGM_RXW_SECS</tt></td>
<td>Set receive window size.</td>
</tr><tr>
<td><tt>PGM_MULTICAST_HOPS</tt></td>
<td>Set maximum number of network hops to cross.</td>
</tr><tr>
<td><tt>PGM_PEER_EXPIRY</tt></td>
<td>Set timeout for removing a dead peer.</td>
</tr><tr>
<td><tt>PGM_SPMR_EXPIRY</tt></td>
<td>Set expiration time of SPM Requests.</td>
</tr><tr>
<td><tt>PGM_NAK_BO_IVL</tt></td>
<td>Set NAK transmit back-off interval.</td>
</tr><tr>
<td><tt>PGM_NAK_RPT_IVL</tt></td>
<td>Set timeout before repeating NAK.</td>
</tr><tr>
<td><tt>PGM_NAK_RDATA_IVL</tt></td>
<td>Set timeout for receiving RDATA.</td>
</tr><tr>
<td><tt>PGM_NAK_DATA_RETRIES</tt></td>
<td>Set retries for DATA packets after NAK.</td>
</tr><tr>
<td><tt>PGM_NAK_NCF_RETRIES</tt></td>
<td>Set retries for DATA after NCF.</td>
</tr>
</table>


## Hints ##
All the example code specifies the default values used by TIBCO Rendezvous, SmartPGM, and Microsoft where appropriate.  They are very general and lenient settings to work on MANs and computers with low resolution timers.  It is recommended to start with the Rendezvous values, as used in all the examples, and adjust to your requirements.  Here follows the basic steps to go through,

## 1:  Choose a suitable window size ##

<tt>PGM_TXW_SQNS</tt>, or<br />
<tt>PGM_TXW_MAX_RTE</tt> and <br />
<tt>PGM_TXW_SECS</tt>.

<tt>PGM_RXW_SQNS</tt>, or<br />
<tt>PGM_RXW_MAX_RTE</tt> and<br />
<tt>PGM_RXW_SECS</tt>.


This can be difficult, Rendezvous defaults to a 60s buffer which will cripple you machine if you publish fast.  Rendezvous can only specify the window size in seconds, with OpenPGM you can specify seconds + bandwidth or individual sequence numbers.  The goal is to have the lowest possible window size that keeps your applications happy.  Too large a window consumes extra memory and can harm your applications cache locality with stale buffers.


## 2:  Choose a maximum packet size or TPDU ##

<tt>PGM_MTU</tt>

The TPDU is the fundamental packet size, send and receive buffers are always allocated with this size.  Receiving packets larger than the set TPDU will truncate the data causing data loss.  Default size is 1,500 bytes, you might want to increase to 9,000 bytes for jumbograms, or reduce to smaller size if you application does not use the entire space.  Reducing the TPDU will reduce the total size of the window, improving cache locality and potentially increase messaging throughput.


## 3:  Dead peer expiration timeout ##

<tt>PGM_PEER_EXPIRY</tt>

How long do you want to wait before dead peers are reaped?  Rendezvous defaults to 300s, if you have publishing applications frequently bouncing up and down this will adversely increase the resource usage in subscribing applications.  Set the value too low and brief network faults will disrupt all the transports bringing all the applications down.


## 4:  Hops or TTL ##

<tt>PGM_MULTICAST_HOPS</tt>

How wide is you network?  By default Rendezvous uses a multicast hop limit of 16, you might wish to increase this for larger networks.


## 5:  Recovery latency, limiting broadcast storms and yappy clients ##


<tt>PGM_NAK_BO_IVL</tt>

<tt>PGM_NAK_RPT_IVL</tt>

<tt>PGM_NAK_RDATA_IVL</tt>

<tt>PGM_NAK_DATA_RETRIES</tt>

<tt>PGM_NAK_NCF_RETRIES</tt>

For most of the time your network is going to not have packet loss, however there is no hard and fast rule for what is acceptable packet loss.  On a very large distribution network you may see 5-10% packet loss for instance.  From the rate of packet loss and business requirements you should be able to determine a latency target for recovered packets.  Business requirements might include how fresh data can be considered, how long before data is stale and can be discarded.

So perfect conditions latency is going to be RTT/2, where RTT means round trip time, the time it takes to go from the source to the destination and back to the source.

Basic recovery is going to be RTT✕3/2 + <tt>NAK_BO_IVL</tt>.

Worst case recovery is RTT✕3/2 + <tt>NAK_BO_IVL</tt> + <tt>NAK_RPT_IVL</tt> ✕ <tt>NAK_NCF_RETRIES</tt> + <tt>NAK_RDATA_IVL</tt> ✕ <tt>NAK_DATA_RETRIES</tt>.

For example, this could be 100us, 50ms, 200s, although the window size is likely to be the lower limit.

The important factor when choosing times is also what the target architecture can reliably provide in terms of resolution, how many cores there are and the likelyhood of context switches.  Do you have a reliable TSC?  Do you have a HPET?  What is the granularity and resolution of event watching wait?

On Microsoft Windows the minimum wait period is by default 16ms, using <tt><a href='http://msdn.microsoft.com/en-us/library/dd757624(v=vs.85).aspx'>timeBeginPeriod</a></tt> this is reduced to 1ms with the tradeoff of increased power usage.  As of OpenPGM 5.1, <tt><a href='OpenPgm5CReferencePgmInit.md'>pgm_init</a></tt> will configure Windows for 1ms timers by default.  Note that Windows sleep functions round down time values which will result in extended busy-wait states which may adversely affect system performance.  It is recommended on Windows to always round timers up to 1ms in order to sleep and yield time to other threads.


## 6:  End of stream notification ##

<tt>PGM_AMBIENT_SPM</tt>

<tt>PGM_HEARTBEAT_SPM</tt>

<tt>PGM_SPMR_EXPIRY</tt>

SPMs are used to keep multicast path in your routing infrastructure, to update clients with the lead sequence of the transmit window, to provide an address to request repairs.  Imagine sending three packets and the last is lost, when is the client aware of that loss?

Imagine late joins, when a client starts and receives packets mid-stream of a publisher.  When packet loss occurs, excluding FEC enabled broadcast-only transports, the client needs the address of the source to request a re-transmission.  Heartbeat SPMs are used to broadcast the source address to clients, SPM-Requests are used to request early transmission of a SPM.

In order for SPMs to be broadcast it is necessary that applications [dispatch protocol events](OpenPgm5CReferenceEvents.md) by using the asynchronous receiver or simply calling <tt><a href='OpenPgm5CReferencePgmRecv.md'>pgm_recv</a></tt> as and when notifications are raised.
