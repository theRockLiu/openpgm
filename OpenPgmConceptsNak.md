<img src='http://miru.hk/wiki/NAK_-_ignore.png' />

  * Reception of invalid NAKs will be ignored.


<img src='http://miru.hk/wiki/NAK_-_confirm.png' />

  * Reception of valid NAKs will be responded with matching NCF, even if the sequence numbers are not in the transmit window.


<img src='http://miru.hk/wiki/NAK_-_repair.png' />

  * Reception of NAK with sequence numbers in the transmit window will be confirmed with a matching NCF and then later RDATA packets for each requested sequence number (ARQ).
