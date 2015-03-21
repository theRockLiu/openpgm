﻿#summary OpenPGM : C Reference : pgm\_async\_recv()
#labels Phase-Implementation

_Function_
### Declaration ###
<pre>
#include <pgm/pgm.h><br>
<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gssize gssize] *pgm_async_recv* (<br>
[OpenPgmCReferencePgmAsyncT pgm_async_t]* const    async,<br>
const [http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gpointer gpointer]        buf,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gsize gsize]                 len,<br>
int                   flags<br>
);<br>
</pre>

### Purpose ###
Receive an Application Protocol Domain Unit (APDU) from an asynchronous queue.

### Remarks ###
<tt>pgm_async_recv()</tt> fills the provided buffer location with up to <tt>len</tt> contiguous APDU bytes  therefore the buffer should be large enough to store the largest APDU expected.

### Parameters ###

<table cellpadding='5' border='1' cellspacing='0'>
<tr>
<th>Parameter</th>
<th>Description</th>
</tr>
<tr>
<td><tt>async</tt></td>
<td>The asynchronous receiver object.</td>
</tr><tr>
<td><tt>buf</tt></td>
<td>Data buffer to fill.</td>
</tr><tr>
<td><tt>len</tt></td>
<td>Length of <tt>buf</tt> in bytes.</td>
</tr><tr>
<td><tt>flags</tt></td>
<td>Bitwise OR of zero or more flags: <tt>MSG_DONTWAIT</tt> enabling non-blocking operation.</td>
</tr>
</table>


### Return Value ###
On success, the number of bytes read from the queue is returned (zero indicates an empty message).  On invalid arguments, <tt>-EINVAL</tt> is returned.  On error, -1 is returned, and <tt>errno</tt> is set appropriately.

### Errors ###
<dl><dt><tt>EAGAIN</tt></dt><dd>The socket is marked non-blocking and the operation would block.<br>
</dd></dl>

### Example ###
Receive an APDU up to 4096 bytes in length.

```
 gchar buf[4096];
 int bytes_read = pgm_async_recv (async, buf, sizeof(buf), 0);
```

### See Also ###
  * <tt><a href='OpenPgmCReferencePgmAsyncT.md'>pgm_async_t</a></tt><br>
<ul><li><tt><a href='OpenPgmCReferencePgmTransportRecv.md'>pgm_transport_recv()</a></tt><br>
</li><li><a href='OpenPgmCReferenceTransport.md'>Transport</a> in OpenPGM C Reference.</li></ul>
