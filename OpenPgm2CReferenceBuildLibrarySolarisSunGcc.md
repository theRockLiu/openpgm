﻿#summary OpenPGM 2 : C Reference : Build Library : Sun GCC 3.4.3 on Solaris (32-bit)
#labels Phase-Implementation
#sidebar TOC2CReferenceProgrammersChecklist
### Building with Sun GCC 3.4.3 for Solaris 10 SPARC ###
First install all the C compiler dependencies, <tt>sudo</tt> and SCons.  GCC and <tt>sudo</tt> are included on the Solaris 10 Companion Software CD and <tt>sudo</tt> needs to be set setuid root and configured with <tt>visudo</tt>.  Scons must be installed with the default Python and not <tt>/usr/sfw/bin/python</tt>.

Note that the provided Sun GCC cannot build 64-bit binaries.

For convenience some executables are symlinked into the path.
<pre>
$ su -<br>
# cd /usr/bin<br>
# chmod +s /usr/sfw/bin/sudo<br>
# ln -s /usr/sfw/bin/sudo<br>
</pre>
Scons must be downloaded from http://sourceforge.net/projects/scons/files/
<pre>
$ /usr/sfw/bin/wget http://sourceforge.net/projects/scons/files/scons/1.2.0/scons-1.2.0.tar.gz/download<br>
$ gzcat scons-1.2.0.tar.gz | tar xvf -<br>
$ cd scons-1.2.0<br>
$ sudo python setup.py install<br>
</pre>
Install the library dependencies, primarily GLib and pkg-config, this tutorial install everything into <tt>/opt/glib-gcc</tt>.  <tt>pkg-config</tt> is a binary and doesn't need to be specially built for v9 architecture.
<pre>
$ /usr/sfw/bin/wget http://pkgconfig.freedesktop.org/releases/pkg-config-0.23.tar.gz<br>
$ gzcat pkg-config-0.23.tar.gz | tar xvf -<br>
$ cd pkg-config-0.23<br>
$ MAKE=gmake AR=gar RANLIB=granlib STRIP=gstrip AS=gas CC=gcc PATH=/usr/sfw/bin:$PATH \<br>
./configure --with-installed-glib --prefix=/opt/glib-gcc<br>
$ PATH=/usr/sfw/bin:$PATH gmake<br>
$ /opt/sfw/bin/sudo PATH=/usr/sfw/bin:$PATH /usr/sfw/bin/gmake install<br>
$ MAKE=gmake AR=gar RANLIB=granlib STRIP=gstrip AS=gas CC=gcc CFLAGS='-m64' \<br>
LIBS='-lresolv -lsocket -lnsl' PATH=/opt/glib-gcc/bin:/usr/sfw/bin:$PATH \<br>
./configure --prefix=/opt/glib-gcc<br>
$ PATH=/usr/sfw/bin:$PATH gmake<br>
$ /opt/sfw/bin/sudo PATH=/usr/sfw/bin:$PATH /usr/sfw/bin/gmake install<br>
</pre>
If you have subversion installed (requires a significant amount of dependencies), checkout the repository as per the [Subversion guide](http://code.google.com/p/openpgm/source/checkout), otherwise download the latest tarball.
<pre>
$ /usr/sfw/bin/wget http://openpgm.googlecode.com/files/libpgm-2.1.26.tar.gz<br>
$ gzcat libpgm-2.1.26.tar.gz | xvf -<br>
</pre>
Build.
<pre>
$ cd release-2-1/openpgm/pgm<br>
$ scons -f SConstruct.Solaris.sungcc<br>
</pre>
By default SCons is configured to build a debug tree in <tt>./ref/debug</tt>:
<pre>
$ scons -f SConstruct.Solaris.sungcc<br>
</pre>
To build the release version in <tt>./ref/release</tt> use the following:
<pre>
$ scons -f SConstruct.Solaris.sungcc BUILD=release<br>
</pre>

#### Testing ####
The dynamic GLib libraries require an extra step for testing, for example,
<pre>
$ sudo LD_LIBRARY_PATH=/opt/glib-gcc/lib:$LD_LIBRARY_PATH ./ref/release/pgmrecv<br>
</pre>

#### Performance Testing ####
Emulating a real world application, <tt>pgmping</tt>, the performance test utility is in C++
and uses Protocol Buffers for the messaging layer.  Download and build with the GNU
toolchain.
<pre>
$ /usr/sfw/bin/wget http://protobuf.googlecode.com/files/protobuf-2.1.0.tar.gz<br>
$ gzcat protobuf-2.1.0.tar.gz | tar xvf -<br>
$ cd protobuf-2.1.0<br>
$ MAKE=gmake AR=gar RANLIB=granlib STRIP=gstrip AS=gas CC=gcc PATH=/usr/sfw/bin:$PATH \<br>
./configure --prefix=/opt/glib-gcc<br>
$ PATH=/usr/sfw/bin:$PATH gmake<br>
$ /opt/sfw/bin/sudo PATH=/usr/sfw/bin:$PATH /usr/sfw/bin/gmake install<br>
</pre>
The build environment needs to be updated with the location of the Protocol Buffers
installation.
<pre>
PROTOBUF_CCFLAGS = '-I/opt/glib-gcc/include',<br>
PROTOBUF_LIBS    = '/opt/glib-gcc/lib/libprotobuf.a',<br>
PROTOBUF_PROTOC  = '/opt/glib-gcc/bin/protoc'<br>
</pre>
Finally build the utility, noting that <tt>pgmping</tt> also depends upon GLib.
<pre>
$ scons -f SConstruct.Solaris.sungcc BUILD=release WITH_PROTOBUF=true<br>
</pre>
Run a PGM reflection server,
<pre>
$ sudo LD_LIBRARY_PATH=/opt/glib-gcc/lib:$LD_LIBRARY_PATH \<br>
./ref/release/examples/pgmping \<br>
-n "eri0;239.192.0.1" -p 3065 -e<br>
</pre>
and run a PGM client,
<pre>
$ sudo LD_LIBRARY_PATH=/opt/glib-gcc/lib:$LD_LIBRARY_PATH \<br>
./ref/release/examples/pgmping \<br>
-n "eri0;239.192.0.1" -p 3065 -m 100<br>
</pre>
