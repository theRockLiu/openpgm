﻿#summary OpenPGM 2 : C Reference : Transport : pgm\_transport\_set\_hops()
#labels Phase-Implementation
#sidebar TOC2CReferenceTransport
_Function_
### Declaration ###
<pre>
#include <pgm/pgm.h><br>
<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gboolean gboolean] *pgm_transport_set_hops* (<br>
[OpenPgm2CReferencePgmTransportT pgm_transport_t]*     transport,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gint gint]                 hops<br>
);<br>
</pre>

### Purpose ###
Set maximum number of network hops to cross.

### Remarks ###
In IPv4, time to live (TTL) is a limit to the number of network boundaries that a multicast packet may cross.  The purpose of TTL is to avoid a situation in which an undeliverable datagram keeps circulating on an internet system, with such a system eventually becoming swamped by such immortal datagrams.  In IPv6 to reflect common practice the TTL field is renamed to hop limit.

Several Cisco routers have a bug causing excessive CPU usage when publishing multicast with a default TTL value of 1, therefore the default value in TIBCO Rendezvous is 16.

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
<td><tt>hops</tt></td>
<td>Hop limit.</td>
</tr>
</table>


### Return Value ###
On success, returns <tt><a href='http://library.gnome.org/devel/glib/stable/glib-Standard-Macros.html#TRUE--CAPS'>TRUE</a></tt>.  On invalid parameter, returns <tt><a href='http://library.gnome.org/devel/glib/stable/glib-Standard-Macros.html#FALSE--CAPS'>FALSE</a></tt>.

### Example ###
```
 pgm_transport_set_hops (transport, 16);
```

### See Also ###
  * <tt><a href='OpenPgm2CReferencePgmTransportT.md'>pgm_transport_t</a></tt><br>
<ul><li><a href='OpenPgm2CReferenceTransport.md'>Transport</a> in OpenPGM C Reference.<br>
</li><li><a href='http://en.wikipedia.org/wiki/Time_to_live'>Time to live</a> in Wikipedia.