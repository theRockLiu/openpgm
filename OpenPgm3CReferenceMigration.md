﻿#summary OpenPGM 3 : C Reference : Migration
#labels Phase-Implementation
#sidebar TOC3CReferenceMigration
## Common Changes ##
### Summary ###
  * Replace <tt><a href='http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html#GError'>GError</a></tt> parameters with [pgm\_error\_t](OpenPgm3CReferencePgmErrorT.md).
  * Return values are now C99 type <tt>bool</tt> instead of GLib based [gboolean](http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gboolean).
  * No requirement to compile or link with GLib.

### Details ###
Version 3.0 removes the dependency upon GLib and so all native GLib data types are replaced with OpenPGM scoped equivalents.

Note that reference counting is now used so that for every <tt><a href='OpenPgm3CReferencePgmInit.md'>pgm_init()</a></tt> there must be a matching <tt><a href='OpenPgm3CReferencePgmShutdown.md'>pgm_shutdown()</a></tt>.

**Version 2.0 code example.**
```
 GError *err = NULL;
 if (!pgm_init (&err)) {
   g_error ("Unable to initialize PGM engine: %s", err->message);
   g_error_free (err);
   return EXIT_FAILURE;
  }
```
**Updated example for version 3.0**
```
 pgm_error_t *err = NULL;
 if (!pgm_init (&err)) {
   fprintf (stderr, "Unable to initialize PGM engine: %s", (err && err->message) ? err->message : "(null)");
   pgm_error_free (err);
   return EXIT_FAILURE;
  }
```


## Transport ##
### Summary ###
  * Replace <tt><a href='OpenPgm2CReferencePgmIoStatus.md'>PGMIOStatus</a></tt> enumerated type with <tt>int</tt>.

### Details ###
For simplificated the enumerated type has been removed and replaced with <tt>int</tt>, the values remain the same.


## Asynchronous Receivers ##
### Summary ###
  * Add <tt>async.c</tt> and <tt>async.h</tt> to your project.

### Details ###
The asynchronous receiver API is no longer supported and is now included as an example usage of the OpenPGM API.


## Monitoring and Administration ##
### Summary ###
  * Remove libsoup compile and link dependencies.


### Details ###
The HTTP administrative interface has been re-written using portable BSD sockets and threading as such no longer depends upon libsoup which also removes the dependency upon GLib.