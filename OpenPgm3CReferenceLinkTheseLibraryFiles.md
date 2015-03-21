﻿#summary OpenPGM 3 C Reference : Link these Library Files
#labels Phase-Implementation
#sidebar TOC3CReferenceProgrammersChecklist
OpenPGM C programs must link the appropriate library files.


<table cellpadding='5' border='1' cellspacing='0'>
<tr>
<th>Linker Flag</th>
<th>Description</th>
</tr>
<tr>
<td><h3>Communications and Events</h3></td>
</tr><tr>
<td><tt>-lpgm</tt></td>
<td>All OpenPGM programs must use this link flag.</td>
</tr><tr>
<td><tt>-lm</tt></td>
<td>Required for support of histogram math enabled via <tt>-DCONFIG_HISTOGRAMS</tt>.</td>
</tr><tr>
<td><tt>-lrt</tt></td>
<td>Required for <tt>clock_gettime()</tt> support.</td>
</tr><tr>
<td><tt>-lresolv -lsocket -lnsl</tt></td>
<td>Required for Solaris socket support.</td>
</tr><tr>
<td><h3>Monitoring and Administration</h3></td>
</tr><tr>
<td><tt>-lpgmhttp </tt></td>
<td>Include this for the HTTP/HTTPS interface.</td>
</tr><tr>
<td><tt>-lpgmsnmp <code>`</code>net-snmp-config --agent-libs<code>`</code></tt></td>
<td>Include this for a Net-SNMP based sub-agent.</td>
</tr>
</table>
