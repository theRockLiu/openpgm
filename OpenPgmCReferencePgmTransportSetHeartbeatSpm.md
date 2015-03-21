﻿﻿#summary OpenPGM : C Reference : pgm\_transport\_set\_heartbeat\_spm()
#labels Phase-Implementation

_Function_
### Declaration ###
<pre>
#include <pgm/pgm.h><br>
<br>
int *pgm_transport_set_heartbeat_spm* (<br>
[OpenPgmCReferencePgmTransportT pgm_transport_t]*     transport,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#guint guint]*               spm_heartbeat_interval,<br>
int                  len<br>
);<br>
</pre>

### Purpose ###
Set intervals of data flushing SPM packets.

### Remarks ###
Heartbeat SPMs are driven by a count-down timer that keeps being reset by data, and the interval of which changes once it begins to expire.  In the presence of data, no heartbeat SPMs will be transmitted.

With the absence of delivery acknowledgement SPMs complement the rolw of data packets in provoking further NAKs from receivers, and maintaining receive window state in the receivers.

For high speed applications the intervals should be small enough to minimise worst case latency, too small reduces the effectiveness of NCFs, DLRs, and can exacerbate packet re-ordering by switched networks.

Specifying intervals lower than can be resolved by the kernel, whether real-time or not, can cause immediate timer firing and NAK generation.  On multiple hosts this can easily instigate a NAK storm, leading to unhappy network elements.

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
<td><tt>spm_heartbeat_interval</tt></td>
<td>Array of Heartbeat SPM broadcast intervals in microseconds.</td>
</tr><tr>
<td><tt>len</tt></td>
<td>Length of <tt>spm_heartbeat_interval</tt>.</td>
</tr>
</table>


### Return Value ###
On success, returns 0.  On invalid parameter, returns <tt>-EINVAL</tt>.

### Example ###
After the last data packet publish a SPM packet in increments of 1ms, 1ms, 2ms, 4ms, 8ms, 16ms, 32ms, 64ms, 128ms, 250ms, 512ms, 1s, 2s, 4s, and finally 8s, matching the ambient SPM interval.

```
 guint spm_heartbeat[] = { 1*1000, 1*1000, 2*1000, 4*1000, 8*1000, 16*1000, 
                           32*1000, 64*1000, 128*1000, 256*1000, 512*1000,
                           1024*1000, 2048*1000, 4096*1000, 8192*1000 };
 pgm_transport_set_heartbeat_spm (transport, spm_heartbeat, G_N_ELEMENTS(spm_heartbeat));
 pgm_transport_set_ambient_spm (transport, 8192*1000);
```

For interoperability with a TIBCO Rendezvous daemon the following intervals can be used.

```
 guint spm_heartbeat[] = { 100*1000, 100*1000, 100*1000, 100*1000, 1300*1000,
                           7*1000*1000, 16*1000*1000, 25*1000*1000, 30*1000*1000 };
 pgm_transport_set_heartbeat_spm (transport, spm_heartbeat, G_N_ELEMENTS(spm_heartbeat));
 pgm_transport_set_ambient_spm (transport, 30*1000*1000);
```

### See Also ###
  * <tt><a href='OpenPgmCReferencePgmTransportT.md'>pgm_transport_t</a></tt><br>
<ul><li><tt><a href='OpenPgmCReferencePgmTransportSetAmbientSpm.md'>pgm_transport_set_ambient_spm()</a></tt><br>
</li><li><a href='OpenPgmCReferenceTransport.md'>Transport</a> in OpenPGM C Reference.