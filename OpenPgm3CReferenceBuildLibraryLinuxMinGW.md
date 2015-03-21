﻿#summary OpenPGM 2 : C Reference : Build Library : GCC 4 for Microsoft Windows (32-bit)
#labels Phase-Implementation
#sidebar TOC3CReferenceProgrammersChecklist
### Building with GCC for 32-bit Microsoft Windows ###
Microsoft Windows is supported by cross-compiling on Ubuntu 9.04 through 10.04 with MinGW.  MSVC9 (C89) is not supported for building OpenPGM however you may build your application and link to a MinGW built OpenPGM library.

First install all the C compiler dependencies, SCons, and Subversion.
<pre>
$ *sudo apt-get install build-essential scons subversion mingw32*<br>
</pre>
Checkout the repository as per the [Subversion guide](http://code.google.com/p/openpgm/source/checkout).
<pre>
$ *svn checkout -r 952 http://openpgm.googlecode.com/svn/trunk *<br>
$ *cd trunk/openpgm/pgm*<br>
</pre>
Patch the mingw32 installation with updated Win32 headers.  For Ubuntu 9.04 or 9.10 use the 3.13 runtime patch,
<pre>
$ *sudo patch -d /usr/i586-mingw32msvc/include < mingw32-runtime_3.13-1openpgm3.diff*<br>
</pre>
For Ubuntu 10.04 use the 3.15 runtime patch,
<pre>
$ *sudo patch -d /usr/i586-mingw32msvc/include < mingw32-runtime_3.15.2-0openpgm1.diff*<br>
</pre>
Build.
<pre>
$ *cd ..*<br>
$ *scons -f SConstruct.mingw*<br>
</pre>
By default SCons is configured to build a debug tree in <tt>./ref/debug-Win32-i686</tt>:

To build the release version in <tt>./ref/release-Win32-i686</tt> use the following:
<pre>
$ *scons -f SConstruct.mingw BUILD=release*<br>
</pre>

#### Testing ####
Testing must be performed with the pure examples unless GLib is installed.  Unicode output may not be displayed correctly depending upon your system settings.
<pre>
C:\pgm>*purinrecv.exe -lp 7500 -n "10.6.15.76;239.192.0.1"*<br>
プリン プリン<br>
Create transport.<br>
Warn: IPv4 adapter MS TCP Loopback interface prefix length is 0, overriding to 32.<br>
Warn: IPv4 adapter MS TCP Loopback interface prefix length is 0, overriding to 32.<br>
Warn: IPv4 adapter MS TCP Loopback interface prefix length is 0, overriding to 32.<br>
Startup complete.<br>
Entering PGM message loop ...<br>
</pre>
Then send a few messages, for example from the same host via loopback
<pre>
c:\pgm\*purinsend.exe -lp 7500 -n "10.6.15.76;239.192.0.1" purin purin*<br>
</pre>
And witness the receiver processing the incoming messages.
<pre>
"purin" (6 bytes from 76.139.60.230.224.49.17539)<br>
"purin" (6 bytes from 76.139.60.230.224.49.17539)<br>
</pre>