﻿#summary OpenPGM 2 : C Reference : Transport : pgm\_recv()
#labels Phase-Implementation
#sidebar TOC2CReferenceTransport
_Function_
### Declaration ###
<pre>
#include <pgm/pgm.h><br>
<br>
[OpenPgm2CReferencePgmIoStatus PGMIOStatus] *pgm_recv* (<br>
[OpenPgm2CReferencePgmTransportT pgm_transport_t]*    transport,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gpointer gpointer]            buf,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gsize gsize]               len,<br>
int                 flags,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gsize gsize]*              bytes_read,<br>
[http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html#GError GError]**            error<br>
);<br>
<br>
[OpenPgm2CReferencePgmIoStatus PGMIOStatus] *pgm_recvfrom* (<br>
[OpenPgm2CReferencePgmTransportT pgm_transport_t]*    transport,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gpointer gpointer]            buf,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gsize gsize]               len,<br>
int                 flags,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gsize gsize]*              bytes_read,<br>
[OpenPgm2CReferencePgmTsiT pgm_tsi_t]*          from,<br>
[http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html#GError GError]**            error<br>
);<br>
<br>
[OpenPgm2CReferencePgmIoStatus PGMIOStatus] *pgm_recvmsg* (<br>
[OpenPgm2CReferencePgmTransportT pgm_transport_t]*    transport,<br>
[OpenPgm2CReferencePgmMsgvT pgm_msgv_t]*         msgv,<br>
int                 flags,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gsize gsize]*              bytes_read,<br>
[http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html#GError GError]**            error<br>
);<br>
</pre>

### Purpose ###
Receive an Application Protocol Domain Unit (APDU) from the transport.

### Remarks ###
The PGM protocol is bi-directional, even send-only applications need to process incoming requests for retransmission (NAKs) and other packet types.  The synchronous API suite provides low level access to the raw events driving the protocol in order to accelerate performance in high message rate environments.

<tt>pgm_recv()</tt> and <tt>pgm_recvfrom()</tt> fill the provided buffer location with up to <tt>len</tt> contiguous APDU bytes therefore the buffer should be large enough to store the largest APDU expected.

If <tt>from</tt> is not <tt>NULL</tt>, the source TSI is filled in.

The <tt>pgm_recv()</tt> is identical to <tt>pgm_recvfrom()</tt> with a <tt>NULL</tt> <tt>from</tt> parameter.

<tt>pgm_recvmsg()</tt> fills a <tt>pgm_msgv_t</tt> structure with a scatter/gather vector of buffers directly from the receive window.  The vector size is governed by IOV\_MAX, on Linux is 1024.  Using the maximum size is not always recommended as time processing the received messages might cause an incoming buffer overrun.

Memory is returned to the receive window on the next call or transport destruction.

Unrecoverable data loss will cause the function to immediately return with <tt>PGM_IO_STATUS_RESET</tt>, setting flags to <tt>MSG_ERR_QUEUE</tt> will return a populated <tt>msgv</tt> with an error skbuff detailing the error.  If <tt><a href='OpenPgm2CReferencePgmTransportSetAbortOnReset.md'>pgm_transport_set_abort_on_reset()</a></tt> is called with <tt>FALSE</tt> (the default mode) processing can continue with subsequent calls to <tt>pgm_recv()</tt>, if <tt>TRUE</tt> then the transport will continue to return <tt>PGM_IO_STATUS_RESET</tt> until destroyed.

It is valid to send and receive zero length PGM packets.

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
<td><tt>len</tt></td>
<td>Length of <tt>buf</tt> in bytes.</td>
</tr><tr>
<td><tt>msgv</tt></td>
<td>Message vector of TPDUs.</td>
</tr><tr>
<td><tt>flags</tt></td>
<td>
<dl><dt><tt>MSG_DONTWAIT</tt></dt><dd>Enables non-blocking operation on Unix, no action on Windows.<br>
</dd><dt><tt>MSG_ERR_QUEUE</tt></dt><dd>Returns an error SKB on session reset.</dd></dl></td>
</tr><tr>
<td><tt>bytes_read</tt></td>
<td>Pointer to store count of bytes read into <tt>buf</tt>.</td>
</tr><tr>
</tr><tr>
<td><tt>error</tt></td>
<td>a return location for a <a href='http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html#GError'>GError</a>, or <a href='http://library.gnome.org/devel/glib/stable/glib-Standard-Macros.html#NULL--CAPS'>NULL</a>.</td>
</tr>
</table>

### Return Value ###
On success, returns <tt>PGM_IO_STATUS_NORMAL</tt>, on error returns <tt>PGM_IO_STATUS_ERROR</tt>, on reset due to unrecoverable data loss, returns <tt>PGM_IO_STATUS_RESET</tt>.  If the transport is marked non-blocking then <tt>PGM_IO_STATUS_WOULD_BLOCK</tt> is returned if the operation would block, if the block would be caused by the rate limiting engine then <tt>PGM_IO_STATUS_RATE_LIMITED</tt> is returned instead.  If no data is available but a pending timer is running <tt>PGM_IO_STATUS_TIMER_PENDING</tt> is returned.

### Example ###
Receive an APDU up to 4096 bytes in length.

```
 gchar buf[4096];
 gsize bytes_read;
 pgm_recv (transport, buf, sizeof(buf), 0, &bytes_read, NULL);
```

Receive an APDU with source TSI.

```
 gchar buf[4096];
 pgm_tsi_t from;
 gsize bytes_read;
 pgm_recvfrom (transport, buf, sizeof(buf), 0, &from, &bytes_read, NULL);
 g_message ("%" G_GSSIZE_FORMAT " bytes received from %s", pgm_tsi_print (&from));
```

Receive a scatter/gather vector APDU message.

```
 pgm_msgv_t msgv;
 gsize bytes_read;
 pgm_recvmsg (transport, &msgv, 0, &bytes_read, NULL);
 printf ("received %d bytes: ", bytes_read);
 struct iovec* msgv_iov = msgv.msgv_iov;
 while (bytes_read > 0) {
   printf ((char*)msgv_iov->iov_base);
   bytes_read -= msgv_iov->iov_len;
   msgv_iov++;
 }
 putchar('\n');
```

Display error details on unrecoverable error.

```
 gchar buf[4096];
 gsize bytes_read;
 GError *err = NULL;
 if (PGM_IO_STATUS_RESET == pgm_recv (transport, buf, sizeof(buf), &bytes_read, &err)) {
   g_warning ("recv error %s", err->message);
   g_error_free (err);
   return EXIT_FAILURE;
 }
```

Display details from error skbuff.

```
 pgm_msgv_t msgv;
 gsize bytes_read;
 if (PGM_IO_STATUS_RESET == pgm_recvmsg (transport, &msgv, MSG_ERR_QUEUE, &bytes_read, NULL)) {
   g_warning ("pgm socket lost %" G_GUINT32_FORMAT " packets detected from %s",
              msgv.msgv_skb[0]->seq,
              pgm_print_tsi(&msgv.msgv_skb[0]->tsi));
   return EXIT_FAILURE;
 }
```

### See Also ###
  * <tt><a href='OpenPgm2CReferencePgmIoStatus.md'>PGMIOStatus</a></tt><br>
<ul><li><tt><a href='OpenPgm2CReferencePgmTransportT.md'>pgm_transport_t</a></tt><br>
</li><li><tt><a href='OpenPgm2CReferencePgmRecvMsgv.md'>pgm_recvmsgv()</a></tt><br>
</li><li><tt><a href='OpenPgm2CReferencePgmTransportCreate.md'>pgm_transport_create()</a></tt><br>
</li><li><a href='OpenPgm2CReferenceTransport.md'>Transport</a> in OpenPGM C Reference.<br>
</li><li><a href='http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html'>GLib Error Reporting</a>.