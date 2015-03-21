OpenPGM supports high message rate delivery, but not all computer and network hardware have the same limits.  Do not write programs that might flood the network with message traffic. High traffic rates may cause intermediary switches and network interface cards to stop high level filtering and pass through all network packets.  When unfiltered packets are passed through to software performance will suffer badly.

Do not code loops that repeatedly send messages without pausing between iterations. Pausing between messages helps leave sufficient network resources for other programs on the network. For example, if your program reads data from a local disk between network operations, it is unlikely to affect any other machines on a reasonably scaled and loaded network; the disk I/O between messages is a large enough pause.

Publishing programs can achieve high throughput rates by sending short bursts of messages punctuated by brief intervals. For example, structure the program as a timer callback function that sends a burst of messages each time its timer triggers; adjust the timer interval and the number of messages per burst for optimal performance.

<pre>
Example: 1 million packets per second (pps) on one port can flood an Alteon layer 2/3 switch<br>
causing it to broadcast all packets to all ports.<br>
</pre>