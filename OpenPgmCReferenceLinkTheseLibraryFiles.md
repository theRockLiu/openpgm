﻿#summary OpenPGM C Reference : Link these Library Files
#labels Phase-Implementation

OpenPGM C programs must link the appropriate library files.


<table cellpadding='5' border='1' cellspacing='0'>
<tr>
<th>Linker Flag</th>
<th>Description</th>
</tr>
<tr>
<td><h3>Communications and Events</h3></td>
</tr><tr>
<td><tt><code>`</code>pkg-config --cflags --libs glib-2.0 gthread-2.0<code>`</code></tt></td>
<td>Include this for the GLib API.</td>
</tr><tr>
<td><tt>-lpgm</tt></td>
<td>All OpenPGM programs must use this link flag.</td>
</tr><tr>
<td><tt>libgcc.a libmingwex.a</tt></td>
<td>GCC compiler dependencies for MSVC from MinGW cross-compile, must appear first in link line.</td>
</tr><tr>
<td><h3>Monitoring and Administration</h3></td>
</tr><tr>
<td>Change the GLib line to<br /><tt><code>`</code>pkg-config --cflags --libs  glib-2.0 gthread-2.0 libsoup-2.2<code>`</code> -lpgmhttp </tt><br /> or libsoup version 2.4 as appropriate,<br /><tt><code>`</code>pkg-config --cflags --libs  glib-2.0 gthread-2.0 libsoup-2.4<code>`</code> -lpgmhttp </tt></td>
<td>Include this for the HTTP/HTTPS interface.</td>
</tr><tr>
<td><tt>-lpgmsnmp <code>`</code>net-snmp-config --agent-libs<code>`</code></tt></td>
<td>Include this for a Net-SNMP based sub-agent.</td>
</tr><tr>
<td><tt>-lm</tt></td>
<td>Required for support of histogram math enabled via <tt>-DCONFIG_HISTOGRAMS</tt>.</td>
</tr>
</table>