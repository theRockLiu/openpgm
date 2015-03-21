﻿#summary OpenPGM 5 : C Reference : Socket : pgm\_getsockname()
#labels Phase-Implementation
#sidebar TOC5CReferenceSocket
_Function_
### Declaration ###
<pre>
#include <pgm/pgm.h><br>
<br>
bool *pgm_getsockname* (<br>
[OpenPgm5CReferencePgmSockT pgm_sock_t]*      const restrict sock,<br>
strict [OpenPgm5CReferencePgmSockAddrT pgm_sockaddr_t]* restrict addr,<br>
socklen_t*             restrict addrlen<br>
);<br>
</pre>

### Purpose ###
Get the PGM socket name.

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
<td><tt>addr</tt></td>
<td>Location to store PGM socket name.</td>
</tr><tr>
<td><tt>addrlen</tt></td>
<td>Value-result of the size of the <tt>addr</tt> parameter.</td>
</tr>
</table>


### Return Value ###
On success, returns <tt>true</tt>.  On invalid parameter, returns <tt>false</tt>.


### See Also ###
  * <tt><a href='OpenPgm5CReferencePgmSockT.md'>pgm_sock_t</a></tt><br>
<ul><li><a href='OpenPgm5CReferenceSocket.md'>Socket</a> in OpenPGM C Reference.