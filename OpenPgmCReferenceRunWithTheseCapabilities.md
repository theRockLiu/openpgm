﻿#summary OpenPGM C Reference : Run With These Capabilities
#labels Phase-Implementation

### Capabilities ###
OpenPGM C programs require system capabilities from this list in order to function.

<table cellpadding='5' border='1' cellspacing='0'>
<tr>
<th>System Capability</th>
<th>Description</th>
</tr>
<tr>
<td><h3>Communications and Events</h3></td>
</tr><tr>
<td><tt>CAP_NET_RAW</tt></td>
<td>To create raw sockets required for the PGM protocol.</td>
</tr><tr>
<td><tt>CAP_SYS_NICE</tt></td>
<td>To set real time thread scheduling and high thread priority for processing incoming messages and high resolution timing.</td>
</tr><tr>
<td><h3>HPET<code>*</code></h3></td>
</tr><tr>
<td>access to <tt>/dev/hpet</tt></td>
<td>The High Performance Event Timer provides a system wide stable monotonic timer.  Default access on Ubuntu is root only.</td>
</tr><tr>
<td><h3>Real-time Clock<code>*</code></h3></td>
</tr><tr>
<td>access to <tt>/dev/rtc</tt></td>
<td>For TSC calibration with the hardware real-time clock access to the <tt>rtc</tt> device can be enabled by adding the user to the real-time Unix group, on Ubuntu this is called <tt>audio</tt>.</td>
</tr>
</table>

**`*` Clock selection** is specified by two system environment variables <tt>PGM_TIMER</tt> governing how to get the current time, and <tt>PGM_SLEEP</tt><sup>†</sup> governing how to wait for an interval of time.


### Clock Selection ###
Kernel timer resolution is in a period of change, kernels can be found with standard resolutions of 1-4ms, with high resolution timers providing microsecond to nanosecond resolution.  <tt><a href='http://en.wikipedia.org/wiki/Rdtsc'>RDTSC</a></tt> is a very cheap mechanism for applications to get the current time in high resolution, however multiple core systems may suffer from drift between each core, some systems may vary frequency in power saving states.  Particularly prone to error are [Hyperthread](http://en.wikipedia.org/wiki/Hyperthread) based processors from Intel, it is highly recommended to disable hyperthreading on all processors wishing to run high speed PGM based messaging.  The High Precision Event Timer, HPET, was created to provide a system wide stable timer in multi-core systems to resolve TSC irregularities.

An operating system calculates the current time of day based off various sources to provide an accurate, stable, and monotonic source to applications.  On Linux the system time is generated from the APIC PM Timer or HPET with interpolation using TSC which can cause the time to become non-monotonic.  Calculating a stable system time is not cheap and historically many applications have been re-engineered to minimise the calls to functions like <tt>gettimeofday()</tt> due to this overhead, using <tt>RDTSC</tt> has been a popular method to bypass this overhead, with multi-core systems either process affinity has to be used to keep using the same TSC source, or reading from the HPET device instead but at a ~500 nanosecond cost.

The RTC device <tt>/dev/rtc</tt> cannot be shared between multiple applications but offers a stable time source external to the TSC and is available on all systems, compared with limited availability of HPET.

On Linux TSC is calibrated using the method defined by <tt>PGM_SLEEP</tt>, and either value can be overridden by defining <tt>RDTSC_FREQUENCY</tt> in kilohertz.  It is recommended to set this frequency or use an alternative timing mechanism on Solaris, and on modern Intel machines with SpeedStep or Turbo technology with variable clock rates.


<table cellpadding='5' border='1' cellspacing='0'>
<tr>
<th>Environment Setting</th>
<th>Description</th>
</tr>
<tr>
<td><h3>PGM_TIMER</h3></td>
</tr><tr>
<td><tt>CLOCK_GETTIME</tt></td>
<td>Use <tt>clock_gettime (CLOCK_MONOTONIC, tp)</tt>, see <tt>CLOCK_GETRES(3)</tt> for further details.</td>
</tr><tr>
<td><tt>FTIME</tt></td>
<td>Use <tt>ftime (tp)</tt>, see <a href='http://www.kernel.org/doc/man-pages/online/pages/man3/ftime.3.html'>ftime(3)</a> for further details.  Windows resolution is limited to 10-16ms.</td>
</tr><tr>
<td><tt>RTC</tt></td>
<td>Read from <tt>/dev/rtc</tt> for current time, a 8192 Hz timer providing 122us resolution.</td>
</tr><tr>
<td><tt>TSC</tt><code>*</code></td>
<td>Call <tt>RDTSC</tt> and normalize with a RTC calibrated count of core ticks.  On Windows uses the APIC PM timer or HPET timer via <a href='http://msdn.microsoft.com/en-us/library/ms644904(VS.85).aspx'>QueryPerformanceCounter</a>.</td>
</tr><tr>
<td><tt>HPET</tt></td>
<td>Read from the <tt>HPET</tt> device which handles femtosecond length pulses.</td>
</tr><tr>
<td><tt>GETTIMEOFDAY</tt><code>*</code></td>
<td>Use <tt>gettimeofday (tv, NULL)</tt>, see <a href='http://www.kernel.org/doc/man-pages/online/pages/man2/gettimeofday.2.html'>gettimeofday(2)</a> for further details.</td>
</tr><tr>
<td><h3>PGM_SLEEP<sup>†</sup></h3></td>
</tr><tr>
<td><tt>CLOCK_NANOSLEEP</tt></td>
<td>Use <tt>clock_nanosleep (clk_id, TIMER_ABSTIME, tp, NULL)</tt>, see <a href='http://www.kernel.org/doc/man-pages/online/pages/man2/clock_nanosleep.2.html'>clock_nanoslepe(2)</a> for further details.</td>
</tr><tr>
<td><tt>NANO_SLEEP</tt></td>
<td>Use <tt>nanosleep (tp, rem)</tt>, see <a href='http://www.kernel.org/doc/man-pages/online/pages/man2/nanosleep.2.html'>nanosleep(2)</a> for further details.</td>
</tr><tr>
<td><tt>RTC</tt></td>
<td>Read interrupts from <tt>/dev/rtc</tt> until requested time has elapsed.</td>
</tr><tr>
<td><tt>TSC</tt></td>
<td>Repeatedly call <tt>RDTSC</tt> interleaving with <a href='http://library.gnome.org/devel/glib/stable/glib-Threads.html#g-thread-yield'>g_thread_yield()</a> to allow other threads to execute.</td>
</tr><tr>
<td><tt>HPET</tt></td>
<td>Repeatedly read from the HPET interleaving with <a href='http://library.gnome.org/devel/glib/stable/glib-Threads.html#g-thread-yield'>g_thread_yield()</a> to allow other threads to execute.</td>
</tr><tr>
<td><tt>PPOLL</tt></td>
<td>Use <tt>ppoll (usec)</tt>, see <a href='http://www.kernel.org/doc/man-pages/online/pages/man2/poll.2.html'>ppoll(2)</a> for further details.</td>
</tr><tr>
<td><tt>USLEEP</tt><code>*</code> or <tt>MICROSLEEP</tt></td>
<td>Use <tt>usleep (usec)</tt>, see <a href='http://www.kernel.org/doc/man-pages/online/pages/man3/usleep.3.html'>usleep(3)</a> for further details.</td>
</tr><tr>
<td><tt>MSLEEP</tt><code>*</code></td>
<td>Use <tt>Sleep (msec)</tt>, see <a href='http://msdn.microsoft.com/en-us/library/ms686298(VS.85).aspx'>Sleep</a> for further details.</td>
</tr><tr>
<td><tt>SELECT</tt></td>
<td>Use <tt>select (usec)</tt>, see <a href='http://www.kernel.org/doc/man-pages/online/pages/man2/select.2.html'>select(2)</a> for further details.</td>
</tr>
</table>

`*` Default settings.
`†` Removed in OpenPGM version 3.0.

### Examples ###
Use the PGM protocol via raw sockets using the <tt>CAP_NET_RAW</tt> capability,
```
$ sudo execcap 'cap_net_raw=ep' pgmsend moo
```

On a single core Intel system, use TSC and <tt>usleep</tt>.
```
$ PGM_TIMER=TSC PGM_SLEEP=USLEEP pgmsend moo
```
or,
```
$ pgmsend moo
```

On a multi-core Intel system with HPET device, use HPET and <tt>usleep</tt>.
```
$ sudo chmod a+r /dev/hpet
$ PGM_TIMER=HPET PGM_SLEEP=USLEEP pgmsend moo
```
or,
```
$ sudo chmod a+r /dev/hpet
$ PGM_TIMER=HPET pgmsend moo
```

On a hyper-threaded Intel system without HPET device, use the RTC device and <tt>usleep</tt>.
```
$ PGM_TIMER=RTC PGM_SLEEP=USLEEP pgmsend moo
```
or,
```
$ PGM_TIMER=RTC pgmsend moo
```

On a multi-core system without HPET and for multiple applications, use <tt>gettimeofday</tt> and <tt>usleep</tt>.
```
$ PGM_TIMER=GTOD PGM_SLEEP=USLEEP pgmsend moo
```
or,
```
$ PGM_TIMER=GTOD pgmsend moo
```

On a Windows system, use the system performance counter and <tt>Sleep</tt>.
```
C:\pgm\bin> set PGM_TIMER=TSC
C:\pgm\bin> set PGM_SLEEP=MSLEEP
C:\pgm\bin> pgmsend moo
```
or,
```
C:\pgm\bin> pgmsend moo
```

On a Solaris system with a 3.2Ghz core, set the TSC frequency first.
```
$ RDTSC_FREQUENCY=3200 PGM_TIMER=TSC PGM_SLEEP=USLEEP pgmsend moo
```
or,
```
$ RDTSC_FREQUENCY=3200 pgmsend moo
```