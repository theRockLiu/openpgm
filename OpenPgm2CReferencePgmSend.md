﻿#summary OpenPGM 2 : C Reference : Transport : pgm\_send()
#labels Phase-Implementation
#sidebar TOC2CReferenceTransport
_Function_
### Declaration ###
<pre>
#include <pgm/pgm.h><br>
<br>
[OpenPgm2CReferencePgmIoStatus PGMIOStatus] *pgm_send* (<br>
[OpenPgm2CReferencePgmTransportT pgm_transport_t]* const    transport,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gconstpointer gconstpointer]             buf,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gsize gsize]                     count,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gsize gsize]*                    bytes_written<br>
);<br>
</pre>

### Purpose ###
Send an Application Protocol Data Unit (APDU) to the network.

### Remarks ###
The underlying transport handles packets sized to the maximum transmission protocol data unit (TPDU), when an application provides a larger APDU it will fragment the message into many packets which the receivers will reconstruct and present as a single data unit.

A would be blocked operation might have sent one or more transmission data units.  The context is saved internally but subsequent call should be either <tt>pgm_send()</tt> with the exact same parameters, or <tt><a href='OpenPgm2CReferencePgmTransportDestroy.md'>pgm_transport_destroy()</a></tt>.  Put another way: in a multi-threaded application a locking mechanism should be implemented to prevent another thread trying to send a different application data unit.

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
<td><tt>buf</tt></td>
<td>Data buffer to fill.</td>
</tr><tr>
<td><tt>count</tt></td>
<td>Length of <tt>buf</tt> in bytes.</td>
</tr><tr>
<td><tt>bytes_written</tt></td>
<td>Pointer to store count of bytes written from <tt>buf</tt>.</td>
</tr>
</table>

### Return Value ###
On success, returns <tt>PGM_IO_STATUS_NORMAL</tt>, on error returns <tt>PGM_IO_STATUS_ERROR</tt>, on reset due to unrecoverable data loss, returns <tt>PGM_IO_STATUS_RESET</tt>.  If the transport is marked non-blocking then <tt>PGM_IO_STATUS_WOULD_BLOCK</tt> is returned if the operation would block, if the block would be caused by the rate limiting engine then <tt>PGM_IO_STATUS_RATE_LIMITED</tt> is returned instead.

### Example ###
Send the traditional Hello world.

```
 gchar* s = "hello world!";
 gsize bytes_written;
 pgm_send (transport, s, strlen(s) + 1, &bytes_written);
```

Basic non-blocking send.

```
 gchar* s = "hello world!";
 size_t len = strlen (s) + 1;
 PGMIOStatus status;
 do {
   status = pgm_send (transport, s, len, NULL);
 } while (PGM_IO_STATUS_WOULD_BLOCK == status ||
          PGM_IO_STATUS_RATE_LIMITED == status);
```

### See Also ###
  * <tt><a href='OpenPgm2CReferencePgmIoStatus.md'>PGMIOStatus</a></tt><br>
<ul><li><tt><a href='OpenPgm2CReferencePgmTransportT.md'>pgm_transport_t</a></tt><br>
</li><li><tt><a href='OpenPgm2CReferencePgmSendv.md'>pgm_sendv()</a></tt><br>
</li><li><a href='OpenPgm2CReferenceTransport.md'>Transport</a> in OpenPGM C Reference.