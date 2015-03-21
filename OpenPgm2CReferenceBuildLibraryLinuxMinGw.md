﻿#summary OpenPGM 2 : C Reference : Build Library : Microsoft Windows (32-bit)
#labels Phase-Implementation
#sidebar TOC2CReferenceProgrammersChecklist
### Building for 32-bit Microsoft Windows ###
Microsoft Windows is supported by cross-compiling on Ubuntu 9.04 through 10.04 with MinGW.  MSVC9 (C89) is not supported for building OpenPGM however you may build your application and link to a MinGW built OpenPGM library.

First install all the C compiler dependencies, SCons, and Subversion.
<pre>
$ sudo apt-get install build-essential scons subversion mingw32<br>
</pre>
Checkout the repository as per the [Subversion guide](http://code.google.com/p/openpgm/source/checkout).
<pre>
$ svn checkout http://openpgm.googlecode.com/svn/branches/release-2-1<br>
$ cd release-2-1/openpgm/pgm<br>
</pre>
Download and unpack the Win32 library dependencies, primarily GLib Binaries & Dev, and proxy-libintl Binaries.  Extract into a subdirectory called <tt>win/</tt>.

  * http://www.gtk.org/download-windows.html
<pre>
$ cd win<br>
$ wget http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.20/glib_2.20.4-1_win32.zip \<br>
http://ftp.gnome.org/pub/gnome/binaries/win32/glib/2.20/glib-dev_2.20.4-1_win32.zip \<br>
http://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/proxy-libintl-dev_20080918_win32.zip<br>
$ for i in *.zip; do unzip $i; done<br>
</pre>
Update the <tt>pkgconfig</tt> configuration for where the root directory is located.
<pre>
$ for i in lib/pkgconfig/*.pc; do sed -i "s#prefix=c:/.*#prefix=`pwd`#" $i; done<br>
</pre>
Patch the mingw32 installation with updated Win32 headers.  For Ubuntu 9.04 or 9.10 use the 3.13 runtime patch,
<pre>
$ sudo patch -d /usr/i586-mingw32msvc/include < mingw32-runtime_3.13-1openpgm3.diff<br>
</pre>
For Ubuntu 10.04 use the 3.15 runtime patch,
<pre>
$ sudo patch -d /usr/i586-mingw32msvc/include < mingw32-runtime_3.15.2-0openpgm1.diff<br>
</pre>
Build.
<pre>
$ cd ..<br>
$ scons -f SConstruct.mingw<br>
</pre>
By default SCons is configured to build a debug tree in <tt>./ref/debug</tt>:
<pre>
$ scons -f SConstruct.mingw<br>
</pre>
To build the release version in <tt>./ref/release</tt> use the following:
<pre>
$ scons -f SConstruct.mingw BUILD=release<br>
</pre>