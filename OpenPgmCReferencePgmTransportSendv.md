﻿#summary OpenPGM : C Reference : pgm\_transport\_sendv()
#labels Phase-Implementation

_Function_
### Declaration ###
<pre>
#include <pgm/pgm.h><br>
<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gssize gssize] *pgm_transport_sendv* (<br>
[OpenPgmCReferencePgmTransportT pgm_transport_t]* const      transport,<br>
const struct iovec*         vector,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#guint guint]                       count,<br>
int                         flags,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gboolean gboolean]                    is_one_apdu,<br>
);<br>
</pre>

### Purpose ###
Send a vector of application buffers as one Application Protocol Data Unit (APDU), or a vector of APDUs to the network.

### Remarks ###
Network delivery of APDUs can be interspersed with ambient SPM and RDATA packets as necessary.

Calling with <tt>MSG_DONTWAIT</tt> flag set will return with <tt>EAGAIN</tt> on any TPDU in the APDU failing to be immediately delivered.  The next call to <tt>pgm_transport_sendv()</tt> MUST be with exactly the same parameters until transmission is complete.

When calling with <tt>MSG_DONTWAIT|MSG_WAIT_ALL</tt> flags you can call with different parameters if <tt>EAGAIN</tt> is returned as non-blocking only occurs at the behest of the defined rate limit.  Individual packets with the APDU are sent blocking mode to ensure delivery of the entire APDU.  It is assumed that sufficient networking testing be deployed such that the application defined rate limit would remove or minimise the chance of blocking to occur at the socket level - it is currently impossible to check whether the underlying network stack could accept all the packets within the provided APDU.

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
<td><tt>vector</tt></td>
<td>Scatter/gather IO vector of data buffers.</td>
</tr><tr>
<td><tt>count</tt></td>
<td>Elements in <tt>vector</tt>.</td>
</tr><tr>
<td><tt>flags</tt></td>
<td>
<dl><dt><tt>MSG_DONTWAIT</tt></dt><dd>Enables TPDU non-blocking operation; if sending a packet would block, <tt>EAGAIN</tt> is returned.<br>
</dd><dt><tt>MSG_DONTWAIT|MSG_WAITALL</tt></dt><dd>Enables APDU non-blocking operation; if sending all packets within the APDU would block due to rate limiting, <tt>EAGAIN</tt> is returned.</dd></dl></td>
</tr><tr>
<td><tt>is_one_apdu</tt></td>
<td><tt>vector</tt> is one APDU if set <tt>TRUE</tt>, <tt>vector::iov_base</tt> is one APDU if set <tt>FALSE</tt>.</td>
</tr>
</table>


### Return Value ###
On success, returns the number of data bytes sent (not transport bytes).  On error, -1 is returned and <tt>errno</tt> set appropriately.

### Errors ###
<dl><dt><tt>EAGAIN</tt></dt><dd>The socket is marked non-blocking and the requested operation would block.<br>
</dd><dt><tt>EINVAL</tt></dt><dd>The sum of the <tt>count</tt> vectors of <tt>iov_len</tt> values overflows an <tt>ssize_t</tt> value. Or, the vector count count is less than zero or greater  than  the  permitted maximum.<br>
</dd><dt><tt>EMSGSIZE</tt></dt><dd>The size of the message to be sent is too large.<br>
</dd><dt><tt>ENOBUFS</tt></dt><dd>The  output queue for a network interface was full.  This generally indicates that the interface has stopped sending, but may be caused by transient congestion.  (Normally, this does not occur in Linux. Packets are just silently dropped when a device queue overflows.)<br>
</dd><dt><tt>ENOMEM</tt></dt><dd>No memory available.<br>
</dd><dt><tt>ECONNRESET</tt></dt><dd>Transport was reset due to unrecoverable data loss with close-on-failure enabled with <tt><a href='OpenPgmCReferencePgmTransportSetCloseOnFailure.md'>pgm_transport_set_close_on_failure()</a>.</tt>.<br>
</dd></dl>

### Example ###
Send one APDU covering two vector buffers.

```
 struct iovec vector[2];
 vector[0].iov_base = "hello ";
 vector[0].iov_len = strlen("hello ");
 vector[1].iov_base = "world!";
 vector[1].iov_len = strlen("world!") + 1;
 pgm_transport_sendv (transport, vector, G_N_ELEMENTS(vector), 0, TRUE);
```

Send two APDUs, one in English, one in Japanese.

```
 struct iovec vector[2];
 vector[0].iov_base = "hello world!";
 vector[0].iov_len = strlen("hello world!") + 1;
 vector[1].iov_base = "おはようございます";
 vector[1].iov_len = wcslen("おはようございます") + 1;  /* bytes not characters */
 pgm_transport_sendv (transport, vector, G_N_ELEMENTS(vector), 0, FALSE);
```

Send with non-blocking APDU with basic event detection.

```
 const char* buffer = "안녕하세요, 잘 지내 시죠?";
 struct iovec vector[2];
 int bytes_sent;
 vector[0].iov_base = buffer;
 vector[0].iov_len = wcslen(buffer) / 2;
 vector[1].iov_base = buffer + (wcslen(buffer) / 2);
 vector[1].iov_len = wcslen(buffer) - (wcslen(buffer) / 2);
 for(;;) {
   bytes_sent = pgm_transport_sendv (transport, vector, G_N_ELEMENTS(vector), MSG_DONTWAIT|MSG_WAITALL, TRUE);
   if (EAGAIN == bytes_sent) {
     int n_fds = 2;
     struct pollfd fds[ n_fds ];
     memset (fds, 0, sizeof(fds));
     pgm_transport_poll_info (transport, fds, &n_fds, POLLIN);
     poll (fds, n_fds, -1 /* timeout=∞ */);
   } else {
     break;
   }
 }
```

### See Also ###
  * <tt><a href='OpenPgmCReferencePgmTransportT.md'>pgm_transport_t</a></tt><br>
<ul><li><tt><a href='OpenPgmCReferencePgmTransportSend.md'>pgm_transport_send()</a></tt><br>
</li><li><tt><a href='OpenPgmCReferencePgmTransportSendPacketv.md'>pgm_transport_send_packetv()</a></tt><br>
</li><li><a href='OpenPgmCReferenceTransport.md'>Transport</a> in OpenPGM C Reference.<br>