﻿#summary OpenPGM 5 : C Reference : Socket : pgm\_sendv()
#labels Phase-Implementation
#sidebar TOC5CReferenceSocket
_Function_
### Declaration ###
<pre>
#include <pgm/pgm.h><br>
<br>
int *pgm_sendv* (<br>
[OpenPgm5CReferencePgmSockT pgm_sock_t]* const       sock,<br>
const struct [OpenPgm5CReferencePgmIoVec pgm_iovec]* vector,<br>
unsigned                count,<br>
bool                    is_one_apdu,<br>
size_t*                 bytes_written<br>
);<br>
</pre>

### Purpose ###
Send a vector of application buffers as one Application Protocol Data Unit (APDU), or a vector of APDUs to the network.

### Remarks ###
Network delivery of APDUs can be interspersed with ambient SPM and RDATA packets as necessary.

Calling with non-blocking transport set will return with <tt>PGM_IO_STATUS_WOULD_BLOCK</tt> or <tt>PGM_IO_STATUS_RATE_LIMITED</tt>. on any TPDU in the APDU failing to be immediately delivered.  The next call to <tt>pgm_sendv()</tt> MUST be with exactly the same parameters until transmission is complete.

### Parameters ###

<table cellpadding='5' border='1' cellspacing='0'>
<tr>
<th>Parameter</th>
<th>Description</th>
</tr>
<tr>
<td><tt>sock</tt></td>
<td>The PGM socket object.</td>
</tr><tr>
<td><tt>vector</tt></td>
<td>Scatter/gather IO vector of data buffers.</td>
</tr><tr>
<td><tt>count</tt></td>
<td>Elements in <tt>vector</tt>.</td>
</tr><tr>
<td><tt>is_one_apdu</tt></td>
<td><tt>vector</tt> is one APDU if set <tt>true</tt>, <tt>vector::iov_base</tt> is one APDU if set <tt>false</tt>.</td>
</tr><tr>
<td><tt>bytes_written</tt></td>
<td>Pointer to store count of bytes written into <tt>buf</tt>.</td>
</tr>
</table>

### Return Value ###
On success, returns <tt>PGM_IO_STATUS_NORMAL</tt>, on error returns <tt>PGM_IO_STATUS_ERROR</tt>.  If the transport is marked non-blocking then <tt>PGM_IO_STATUS_WOULD_BLOCK</tt> is returned if the operation would block, if the block would be caused by the rate limiting engine then <tt>PGM_IO_STATUS_RATE_LIMITED</tt> is returned instead.

If PGMCC is enabled and congestion occurs <tt>PGM_IO_STATUS_CONGESTION</tt> is returned, the application should wait o
n notification from the ACK channel before re-attempting to send.

### Example ###
Send one APDU covering two vector buffers.

```
 struct pgm_iovec vector[2];
 vector[0].iov_base = "hello ";
 vector[0].iov_len = strlen ("hello ");
 vector[1].iov_base = "world!";
 vector[1].iov_len = strlen ("world!") + 1;
 pgm_sendv (sock, vector, 2, true, NULL);
```

Send two APDUs, one in English, one in Japanese.

```
 struct pgm_iovec vector[2];
 vector[0].iov_base = "hello world!";
 vector[0].iov_len = strlen ("hello world!") + 1;
 vector[1].iov_base = "おはようございます";
 vector[1].iov_len = wcslen ("おはようございます") + 1;  /* bytes not characters */
 pgm_sendv (sock, vector, 2, false, NULL);
```

Send with non-blocking APDU with basic event detection.

```
 const char* buffer = "안녕하세요, 잘 지내 시죠?";
 struct pgm_iovec vector[2];
 int status;
 size_t bytes_sent;
 vector[0].iov_base = buffer;
 vector[0].iov_len = wcslen(buffer) / 2;
 vector[1].iov_base = buffer + (wcslen(buffer) / 2);
 vector[1].iov_len = wcslen(buffer) - (wcslen(buffer) / 2);
 for(;;) {
   status = pgm_sendv (sock, vector, 2, true, &bytes_sent);
   if (PGM_IO_STATUS_WOULD_BLOCK == status) {
     int n_fds = 3;
     struct pollfd fds[ n_fds ];
     memset (fds, 0, sizeof(fds));
     pgm_poll_info (sock, fds, &n_fds, POLLIN);
     poll (fds, n_fds, -1 /* timeout=∞ */);
   } else {
     break;
   }
 }
```

### See Also ###
  * <tt><a href='OpenPgm5CReferencePgmIoStatus.md'>PGMIOStatus</a></tt><br>
<ul><li><tt><a href='OpenPgm5CReferencePgmSockT.md'>pgm_sock_t</a></tt><br>
</li><li><tt><a href='OpenPgm5CReferencePgmSend.md'>pgm_send()</a></tt><br>
</li><li><tt><a href='OpenPgm5CReferencePgmSendSkbv.md'>pgm_send_skbv()</a></tt><br>
</li><li><a href='OpenPgm5CReferenceSocket.md'>Socket</a> in OpenPGM C Reference.<br>