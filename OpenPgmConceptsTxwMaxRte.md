_Setting_
### Syntax ###
<table cellpadding='5' border='1' cellspacing='0'>
<tr>
<td>Name</td>
<td><tt>TXW_MAX_RTE</tt></td>
</tr><tr>
<td>SNMP identifier</td>
<td><tt>pgmSourceTxwMaxRte</tt></td>
</tr><tr>
<td>Range</td>
<td><tt>1..4294967295U (uint32)</tt></td>
</tr>
</table>


### Purpose ###
The maximum transmit rate in bytes/second of a source.

### Remarks ###
In addition to defining the transmit window size <tt>TXW_MAX_RTE</tt> enables the rate regulation engine which uses <tt>TXW_MAX_RTE</tt> as the maximum cumulative transmit rate of SPM, ODATA, and RDATA packets.  The rate engine calculation is based on PGM envelope size and a guesstimate on the IP header size (evaluation of sizeof(iphdr), or 20 bytes for IPv4, and 40 bytes for IPv6).  Note that actual IP header size can be different in certain network conditions.

There is no rate limiting in TIBCO Rendezvous, in Smart PGM default <tt>TXW_MAX_RTE</tt> is 1024000.

The following packet types are unaffected: POLL, POLR, NAK, NNAK, NCF, and SPMR.

### See Also ###
  * <tt><a href='OpenPgmConceptsRxwMaxRte.md'>RXW_MAX_RTE</a>]</tt><br>
<ul><li><tt><a href='OpenPgmConceptsTxwSecs.md'>TXW_SECS</a>]</tt><br>
</li><li><tt><a href='OpenPgmConceptsTxwSqns.md'>TXW_SQNS</a>]</tt><br>
</li><li><tt><a href='OpenPgmCReferencePgmTransportSetTxwMaxRte.md'>pgm_transport_set_txw_max_rte()</a>]</tt> in OpenPGM C Reference.</li></ul>
