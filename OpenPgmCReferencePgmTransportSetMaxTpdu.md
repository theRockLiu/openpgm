﻿#summary OpenPGM : C Reference : pgm\_transport\_set\_max\_tpdu()
#labels Phase-Implementation

_Function_
### Declaration ###
<pre>
#include <pgm/pgm.h><br>
<br>
int *pgm_transport_set_max_tpdu* (<br>
[OpenPgmCReferencePgmTransportT pgm_transport_t]*     transport,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#guint16 guint16]              max_tpdu<br>
);<br>
</pre>

### Purpose ###
Set maximum transport data unit size.

### Remarks ###
The maximum transport protocol data unit (TPDU) size is equivalent to the raw packet frame size, usually matching the systems maximum transmission unit (MTU) typically 1,500 bytes for ethernet to 9,000 for common jumbo-frames.  This value might be lowered in certain environments to support framing extensions such as [802.2 SNAP LLC](http://en.wikipedia.org/wiki/802.2).

When enabling UDP encapsulation remember to include the datagram header size for the data unit size, standard sizes are 20 bytes for IPv4 and 48 bytes for IPv6.

The default maximum TPDU size in TIBCO Rendezvous PGM is 1492 bytes.

The actual maximum application protocol data unit (APDU) size per packet varies with the PGM options in effect, the following table shows the reductions in size that can occur.


<table cellpadding='5' border='1' cellspacing='0'>
<tr>
<th>Size</th>
<th>Description</th>
</tr>
<tr>
<td>-8 bytes</td>
<td>802.2 SNAP LLC header for subnets.</td>
</tr><tr>
<td>-20 bytes</td>
<td>UDP encapsulation header.</td>
</tr><tr>
<td>-24 bytes</td>
<td>PGM Data Packet header.</td>
</tr><tr>
<td><h3>Option Extensions</h3></td>
</tr><tr>
<td>-4 bytes</td>
<td>Option extension length.</td>
</tr><tr>
<td>-16 bytes</td>
<td>Fragmentation Option. †</td>
</tr><tr>
<td>-8 bytes</td>
<td>Late Joining Option. ‡</td>
</tr><tr>
<td>-4 bytes</td>
<td>Synchronisation Option (first packet and its repairs). ‡</td>
</tr><tr>
<td>-4 bytes</td>
<td>Session Finish Option (last packet and its repairs). ‡</td>
</tr><tr>
<td>-8 bytes</td>
<td>Parity Group Number Option. §</td>
</tr><tr>
<td>-8 bytes</td>
<td>Actual Transmission Group Size Option. §‡</td>
</tr><tr>
<td>-2 bytes</td>
<td>Variable TSDU length encoding for parity packets †</td>
</tr><tr>
<td><b>-102 bytes</b></td>
<td><b>Worst case overhead.</b></td>
</tr>
</table>

<dl><dt>†</dt><dd>For APDUs greater than one TPDU in length, the per packet overhead is increased.<br>
</dd><dt>‡</dt><dd>Not implemented.<br>
</dd><dt>§</dt><dd>When Forward-Error Correction is enabled.<br>
</dd></dl>

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
<td><tt>max_tpdu</tt></td>
<td>Maximum transport protocol data unit (TPDU) size.</td>
</tr>
</table>


### Return Value ###
On success, returns 0.  On invalid parameter, returns <tt>-EINVAL</tt>.

### Example ###
```
 pgm_transport_set_max_tpdu (transport, 1500);
```

### See Also ###
  * <tt><a href='OpenPgmCReferencePgmTransportT.md'>pgm_transport_t</a></tt><br>
<ul><li><a href='OpenPgmCReferenceTransport.md'>Transport</a> in OpenPGM C Reference.<br>
</li><li><a href='http://en.wikipedia.org/wiki/802.2'>IEEE 802.2</a> in Wikipedia.<br>
</li><li><a href='http://en.wikipedia.org/wiki/Jumbo_frame'>Jumbo frame</a> in Wikipedia.<br>
</li><li><a href='http://en.wikipedia.org/wiki/Maximum_transmission_unit'>Maximum transmission unit</a> in Wikipedia.