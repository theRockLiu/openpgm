This section describes the network layout necessary to perform PGM protocol compliance of a PGM network stack.

## Basic Testing ##
Basic covers the principal operations of sending, receiving, error detection, requesting packet retransmission (NAKs), and  transmission of repair data.

<img src='http://miru.hk/wiki/OpenPGM_testing_layout.png' />

The network layout described covers four hosts:

<dl><dt>Application</dt><dd>Runs the PGM network stack to test.  This is implemented as a thin agnostic layer above the API allowing control and feedback to the tester.<br>
</dd><dt>Simulator</dt><dd>A host which can send and receive PGM packets in order to illicit a response from the application under test.<br>
</dd><dt>Monitor</dt><dd>An independent network monitor to verify what is actually sent on the network.<br>
</dd><dt>Tester</dt><dd>The central control point which co-ordinates the start and operation of the Application, Simulator, and Monitor systems.<br>
</dd></dl>

The diagram suggests the tester as a human operator but it is possible to run as an automated <tt>expect</tt> style script to simplify repeated test validations.  An example flow for NAK testing is as follows:

<table>
<tr><td>
<table cellpadding='5' border='1' cellspacing='0'>
<tr>
<td><a href='http://miru.hk/wiki/Flow_part1.png'><img src='http://miru.hk/wiki/320px-Flow_part1.png' /><br /><br /><img src='http://miru.hk/wiki/magnify-clip.png' align='right' /></a><font size='1'>1: Create transports in both Application and Simulator</font></td>
</tr>
</table>
</td><td>
<table cellpadding='5' border='1' cellspacing='0'>
<tr>
<td><a href='http://miru.hk/wiki/Flow_part2.png'><img src='http://miru.hk/wiki/320px-Flow_part2.png' /><br /><br /><img src='http://miru.hk/wiki/magnify-clip.png' align='right' /></a><font size='1'>2: Send messages from Application</font></td>
</tr>
</table>
</td></tr><tr><td>
<table cellpadding='5' border='1' cellspacing='0'>
<tr>
<td><a href='http://miru.hk/wiki/Flow_part3.png'><img src='http://miru.hk/wiki/320px-Flow_part3.png' /><br /><br /><img src='http://miru.hk/wiki/magnify-clip.png' align='right' /></a><font size='1'>3: Send NAK from Simulator</font></td>
</tr>
</table>
</td><td>
<table cellpadding='5' border='1' cellspacing='0'>
<tr>
<td><a href='http://miru.hk/wiki/Flow_part4.png'><img src='http://miru.hk/wiki/320px-Flow_part4.png' /><br /><br /><img src='http://miru.hk/wiki/magnify-clip.png' align='right' /></a><font size='1'>4: Wait for RDATA data re-transmission from Application</font></td>
</tr>
</table>
</td></tr></table>
