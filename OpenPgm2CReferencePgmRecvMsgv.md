﻿#summary OpenPGM 2 : C Reference : Transport : pgm\_recvmsgv()
#labels Phase-Implementation
#sidebar TOC2CReferenceTransport
_Function_
### Declaration ###
<pre>
#include <pgm/pgm.h><br>
<br>
[OpenPgm2CReferencePgmIoStatus PGMIOStatus] *pgm_recvmsgv* (<br>
[OpenPgm2CReferencePgmTransportT pgm_transport_t]*      transport,<br>
[OpenPgm2CReferencePgmMsgvT pgm_msgv_t]* const     msgv,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gsize gsize]                 count,<br>
int                   flags,<br>
[http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gsize gsize]*                bytes_read,<br>
[http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html#GError GError]**              error<br>
);<br>
</pre>

### Purpose ###
Receive a vector of Application Protocol Domain Unit's (APDUs) from the transport.

### Remarks ###
The PGM protocol is bi-directional, even send-only applications need to process incoming requests for retransmission (NAKs) and other packet types.  The synchronous API suite provides low level access to the raw events driving the protocol in order to accelerate performance in high message rate environments.

<tt>pgm_recvmsgv()</tt> fills a vector of <tt>pgm_msgv_t</tt> structures with a scatter/gather vector of buffers directly from the receive window.  The vector size is governed by IOV\_MAX, on Linux is 1024, therefore up to 1024 TPDUs or 1024 messages may be returned, whichever is the lowest.  Using the maximum size is not always recommended as time processing the received messages might cause an incoming buffer overrun.

Memory is returned to the receive window on the next call or transport destruction.

Unrecoverable data loss will cause the function to immediately return with <tt>PGM_IO_STATUS_RESET</tt>, setting flags to <tt>MSG_ERR_QUEUE</tt> will return a populated <tt>msgv</tt> with an error skbuff detailing the error.  If <tt><a href='OpenPgm2CReferencePgmTransportSetAbortOnReset.md'>pgm_transport_set_abort_on_reset()</a></tt> is called with <tt>FALSE</tt> (the default mode) processing can continue with subsequent calls to <tt>pgm_recv()</tt>, if <tt>TRUE</tt> then the transport will return <tt>PGM_IO_STATUS_RESET</tt> until destroyed.

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
<td><tt>msgv</tt></td>
<td>Message vector of TPDU vectors.</td>
</tr><tr>
<td><tt>count</tt></td>
<td>Elements in <tt>msgv</tt>.</td>
</tr><tr>
<td><tt>flags</tt></td>
<td>
<dl><dt><tt>MSG_DONTWAIT</tt></dt><dd>Enables non-blocking operation on Unix, no action on Windows.<br>
</dd><dt><tt>MSG_ERR_QUEUE</tt></dt><dd>Returns an error SKB on session reset.</dd></dl></td>
</tr><tr>
<td><tt>bytes_read</tt></td>
<td>Pointer to store count of bytes read into <tt>msgv</tt>.</td>
</tr><tr>
</tr><tr>
<td><tt>error</tt></td>
<td>a return location for a <a href='http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html#GError'>GError</a>, or <a href='http://library.gnome.org/devel/glib/stable/glib-Standard-Macros.html#NULL--CAPS'>NULL</a>.</td>
</tr>
</table>

### Return Value ###
On success, returns <tt>PGM_IO_STATUS_NORMAL</tt>, on error returns <tt>PGM_IO_STATUS_ERROR</tt>, on reset due to unrecoverable data loss, returns <tt>PGM_IO_STATUS_RESET</tt>.  If the transport is marked non-blocking then <tt>PGM_IO_STATUS_WOULD_BLOCK</tt> is returned if the operation would block, if the block would be caused by the rate limiting engine then <tt>PGM_IO_STATUS_RATE_LIMITED</tt> is returned instead.  If no data is available but a pending timer is running <tt>PGM_IO_STATUS_TIMER_PENDING</tt> is returned.

### Example ###
Read a maximum size vector from a transport.

```
 gsize iov_max = sysconf( SC_IOV_MAX );
 pgm_msgv_t msgv[iov_max];
 gsize bytes_read;
 pgm_recvmsgv (transport, msgv, iov_max, 0, &bytes_read, NULL);
```

Display error details on unrecoverable error.

```
 pgm_msgv_t msgv[10];
 gsize bytes_read;
 GError* err = NULL;
 if (PGM_IO_STATUS_RESET == pgm_recvmsgv (transport, msgv, G_N_ELEMENTS(msgv), 0, &bytes_read, &err)) {
   g_warning ("recv: %s", err->message);
   g_error_free (err);
   return EXIT_FAILURE;
 }
```

Take ownership of a PGM SKB by incrementing the reference count with <tt><a href='OpenPgm2CReferencePgmAllocSkb.md'>pgm_skb_get()</a></tt>.

```
 pgm_msgv_t msgv[1];
 struct pgm_sk_buff_t* skb = NULL;
 if (PGM_IO_STATUS_NORMAL == pgm_recvmsgv (transport, msgv, 1, 0, NULL, NULL))
 {
   if (msgv[0].msgv_skb[0]->len > 1 &&
       msgv[0].msgv_skb[0]->data[0] == MAGIC_VALUE)
   {
     skb = pgm_skb_get (msgv[0].msgv_skb[0];
   }
 }
```

### See Also ###
  * <tt><a href='OpenPgm2CReferencePgmIoStatus.md'>PGMIOStatus</a></tt><br>
<ul><li><tt><a href='OpenPgm2CReferencePgmTransportT.md'>pgm_transport_t</a></tt><br>
</li><li><tt><a href='OpenPgm2CReferencePgmRecv.md'>pgm_recv()</a></tt><br>
</li><li><tt><a href='OpenPgm2CReferencePgmTransportCreate.md'>pgm_transport_create()</a></tt><br>
</li><li><a href='OpenPgm2CReferenceTransport.md'>Transport</a> in OpenPGM C Reference.<br>
</li><li><a href='http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html'>GLib Error Reporting</a>.