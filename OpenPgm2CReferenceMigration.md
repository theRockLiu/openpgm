﻿#summary OpenPGM 2 : C Reference : Migration
#labels Phase-Implementation
#sidebar TOC2CReferenceMigration
## Environment ##
### Summary ###
  * Add <tt><a href='http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html#GError'>GError</a></tt> parameter to <tt><a href='OpenPgm2CReferencePgmInit.md'>pgm_init()</a></tt>.
  * Add call to <tt><a href='OpenPgm2CReferencePgmShutdown.md'>pgm_shutdown()</a></tt> on application shutdown.

### Details ###
Version 2.0 introduces a <tt><a href='OpenPgm2CReferencePgmShutdown.md'>pgm_shutdown()</a></tt> function to free resources used by the PGM engine together with <tt><a href='OpenPgm2CReferencePgmSupported.md'>pgm_supported()</a></tt>, a method to detect previously initialized PGM engine allowing multiple subsystems to use PGM, for example within a library and inside an application core.

Additional error reporting on version 2 adds <tt><a href='http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html#GError'>GError</a></tt> return values to all major functions, this extra information can be ignored by passing a NULL value.

**Version 1.0 code example.**
```
 pgm_init();
```
**Updated example for version 2.0.**
```
 GError *err = NULL;
 if (!pgm_init (&err)) {
   g_error ("Unable to initialize PGM engine: %s", err->message);
   g_error_free (err);
   return EXIT_FAILURE;
  }
```
**Ignoring new error reporting.**
```
 if (!pgm_init (NULL)) {
   g_error ("Unable to initialize PGM engine");
   return EXIT_FAILURE;
  }
```

## Interface and GSI ##
### Summary ###
  * Replace interface parsing calls to <tt><a href='OpenPgmCReferencePgmIfParseTransport.md'>pgm_if_parse_transport()</a></tt> with <tt><a href='OpenPgm2CReferencePgmIfGetTransportInfo.md'>pgm_if_get_transport_info()</a></tt>.
  * Parameters <tt>recv_gsr</tt>, <tt>recv_len</tt>, and <tt>send_gsr</tt> need to be replaced with an object of structure <tt><a href='OpenPgm2CReferencePgmTransportInfoT.md'>pgm_transport_info_t</a></tt>.
  * Replace GSI creation calls to <tt><a href='OpenPgmCReferencePgmCreateIPv4Gsi.md'>pgm_create_ipv4_gsi()</a></tt> and <tt><a href='OpenPgmCReferencePgmCreateMD5Gsi.md'>pgm_create_md5_gsi()</a></tt> with <tt><a href='OpenPgm2CReferencePgmGsiCreateFromAddr.md'>pgm_gsi_create_from_addr()</a></tt> and <tt><a href='OpenPgm2CReferencePgmGsiCreateFromHostname.md'>pgm_gsi_create_from_hostname()</a></tt> respectively.


### Details ###
Version 2.0 simplifies the parameters for creating a PGM transport by combining all the network details into one structure, <tt><a href='OpenPgm2CReferencePgmTransportInfoT.md'>pgm_transport_info_t</a></tt>.  The structure can be manually populated or filled with a helper function <tt><a href='OpenPgm2CReferencePgmIfGetTransportInfo.md'>pgm_if_get_transport_info()</a></tt>.

GSI creation functions <tt><a href='OpenPgmCReferencePgmCreateIPv4Gsi.md'>pgm_create_ipv4_gsi()</a>]</tt> and <tt><a href='OpenPgmCReferencePgmCreateMD5Gsi.md'>pgm_create_md5_gsi()</a></tt> and been replaced with variants including extra error reporting and normalized names, <tt><a href='OpenPgm2CReferencePgmGsiCreateFromAddr.md'>pgm_gsi_create_from_addr()</a></tt> and <tt><a href='OpenPgm2CReferencePgmGsiCreateFromHostname.md'>pgm_gsi_create_from_hostname()</a></tt> respectively.  These new functions populate contents of the <tt><a href='OpenPgm2CReferencePgmTransportInfoT.md'>pgm_transport_info_t</a></tt> structure and if used, should be called after <tt><a href='OpenPgm2CReferencePgmIfGetTransportInfo.md'>pgm_if_get_transport_info()</a></tt>.

Transport info structures created with <tt><a href='OpenPgm2CReferencePgmIfGetTransportInfo.md'>pgm_if_get_transport_info()</a></tt> should be freed with <tt><a href='OpenPgm2CReferencePgmIfGetTransportInfo.md'>pgm_if_free_transport_info()</a></tt>.  The transport info object can be released without needing to destroy any associated PGM transport.

Improved error reporting means that detailed error messages are available on invalid network parameters.  Using the additional error results is optional.


**Example version 1.0 code using UDP encapsulation and hostname based GSI.**
```
 pgm_gsi_t gsi;
 struct group_source_req recv_gsr, send_gsr;
 gsize recv_len;
 int e;
 
 e = pgm_create_md5_gsi (&gsi);
 g_assert (e == 0);
 recv_len = 1;
 e = pgm_if_parse_transport (network, AF_UNSPEC, &recv_gsr, &recv_len, &send_gsr);
 g_assert (e == 0);
 g_assert (recv_len == 1);
 /* populate UDP encapsulation port */
 ((struct sockaddr_in*)&send_gsr.gsr_group)->sin_port = g_htons (udp_encap_port);
 ((struct sockaddr_in*)&recv_gsr.gsr_group)->sin_port = g_htons (udp_encap_port);
```

**Example updated for version 2 with basic error handling.**
```
 struct pgm_transport_info_t* res = NULL;
 gboolean e;
 
 e = pgm_if_get_transport_info (network, NULL, &res, NULL);
 g_assert (TRUE == e);
 e = pgm_gsi_create_from_hostname (&res->ti_gsi, NULL);
 g_assert (TRUE == e);
 /* populate UDP encapsulation port */
 res->ti_udp_encap_ucast_port = udp_encap_port;
 res->ti_udp_encap_mcast_port = udp_encap_port;
```

**Example updated for version 2 with detailed error reporting.**
```
 struct pgm_transport_info_t* res = NULL;
 GError* err = NULL;
 
 if (!pgm_if_get_transport_info (network, NULL, &res, &err)) {
   g_error ("Parsing network parameter: %s", err->message);
   g_error_free (err);
   return EXIT_FAILURE;
 }
 if (!pgm_gsi_create_from_hostname (&res->ti_gsi, &err))) {
   g_error ("Creating GSI: %s", err->message);
   g_error_free (err);
   pgm_if_free_transport_info (res);
   return EXIT_FAILURE;
 }
 /* populate UDP encapsulation port */
 res->ti_udp_encap_ucast_port = udp_encap_port;
 res->ti_udp_encap_mcast_port = udp_encap_port;
```

## Transport ##
### Summary ###
  * Update transport create and bind functions with <tt><a href='http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html#GError'>GError</a></tt> parameters.
  * <tt>Update pgm_transport_recv</tt> calls with shortened name version, <tt><a href='http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html#GError'>GError</a></tt> parameters, and <tt><a href='http://library.gnome.org/devel/glib/stable/glib-IO-Channels.html#GIOStatus'>GIOStatus</a></tt> return values.
  * Update transport parameter setters to check for <tt><a href='http://library.gnome.org/devel/glib/stable/glib-Basic-Types.html#gboolean'>gboolean</a></tt> return values instead of <tt>int</tt> based ones.
  * Non-blocking flag <tt>MSG_DONTWAIT</tt> needs to be replaced with transport wide <tt><a href='OpenPgm2CReferencePgmTransportSetNonBlocking.md'>pgm_transport_set_nonblocking()</a></tt> for support on Windows platforms.
  * Remove setters for transmit or receive window pre-allocation sizes.


### Details ###
Version 2.0 introduces more detailed error reporting on network operations so every transport API, send-side excepting includes an optional <tt><a href='http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html#GError'>GError</a></tt> return value.


**Example version 1.0 code.**
```
 char buffer[4096];
 gsize bytes_read, bytes_sent;
 pgm_transport_t *transport;
 int e;
 
 e = pgm_transport_create (&transport, &gsi, dport, sport, &recv_gsr, recv_len, &send_gsr);
 g_assert (e == 0);
 pgm_transport_set_max_tpdu (transport, max_tpdu);
 pgm_transport_set_txw_sqns (transport, sqns);
 pgm_transport_set_txw_max_rte (transport, max_rte);
 pgm_transport_set_multicast_loop (transport, multicast_loop);
 pgm_transport_set_hops (transport, hops);
 pgm_transport_set_ambient_spm (transport, spm_ambient);
 pgm_transport_set_heartbeat_spm (transport, spm_heartbeat, G_N_ELEMENTS(spm_heartbeat));
 e = pgm_transport_bind (transport);
 g_assert (e == 0);
 bytes_read = pgm_transport_recv (transport, buffer, sizeof(buffer), 0);
 if (bytes_read >= 0)
   g_message ("recv: %s", buffer);
 bytes_sent = pgm_transport_send (transport, buffer, bytes_read, 0);
 if (bytes_sent >= 0)
   g_message ("sent: %d bytes", bytes_sent);
```

**Updated code for version 2.0.**
```
 pgm_transport_t *transport;
 char buffer[4096];
 gsize bytes_read, bytes_sent;
 PGMIOStatus status;
 GError* err = NULL;
 
 if (!pgm_transport_create (&transport, tinfo, &err)) {
   g_error ("creating transport: %s", err->message);
   g_error_free (err);
   pgm_if_free_transport_info (tinfo);
   return EXIT_FAILURE;
 }
 pgm_if_free_transport_info (tinfo);
 pgm_transport_set_max_tpdu (transport, max_tpdu);
 pgm_transport_set_txw_sqns (transport, sqns);
 pgm_transport_set_txw_max_rte (transport, max_rte);
 pgm_transport_set_multicast_loop (transport, multicast_loop);
 pgm_transport_set_hops (transport, hops);
 pgm_transport_set_ambient_spm (transport, spm_ambient);
 pgm_transport_set_heartbeat_spm (transport, spm_heartbeat, G_N_ELEMENTS(spm_heartbeat));
 if (!pgm_transport_bind (transport, &err)) {
   g_error ("binding transport: %s", err->message);
   g_error_free (err);
   pgm_transport_destroy (transport, FALSE);
   return EXIT_FAILURE;
 }
 status = pgm_recv (transport, buffer, sizeof(buffer), 0, &bytes_read, &err);
 if (PGM_IO_STATUS_NORMAL == status)
   g_message ("recv: %s", buffer);
 status = pgm_transport_send (transport, buffer, bytes_read, &bytes_sent);
 if (PGM_IO_STATUS_NORMAL == status)
   g_message ("sent: %d bytes", bytes_sent);
```

## Asynchronous Receivers ##
### Summary ###
  * Update calls to <tt><a href='OpenPgm2CReferencePgmAsyncCreate.md'>pgm_async_create()</a></tt> with optional <tt><a href='http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html#GError'>GError</a></tt> parameter.
  * Update calls to <tt><a href='OpenPgm2CReferencePgmAsyncRecv.md'>pgm_async_recv()</a>]</tt> with <tt><a href='http://library.gnome.org/devel/glib/stable/glib-IO-Channels.html#GIOStatus'>GIOStatus</a></tt> return value and optional <tt><a href='http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html#GError'>GError</a></tt> parameter.


### Details ###
Version 2.0 introduces more detailed error reporting on network and threading operations so async creation and recv calls include optional <tt><a href='http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html#GError'>GError</a></tt> return values.


## Zero Copy ##
### Summary ###
  * Update <tt>packetv</tt> structures with <tt>struct <a href='OpenPgm2CReferencePgmSkBuffT.md'>pgm_sk_buff_t</a></tt>.
  * Replace <tt><a href='OpenPgmCReferencePgmPacketvAlloc.md'>pgm_packetv_alloc()</a></tt> and <tt><a href='OpenPgmCReferencePgmPacketvFree1.md'>pgm_packetv_free1()</a></tt> with <tt><a href='OpenPgm2CReferencePgmAllocSkb.md'>pgm_alloc_skb()</a></tt> and <tt><a href='OpenPgm2CReferencePgmAllocSkb.md'>pgm_free_skb()</a></tt>.
  * Replace <tt><a href='OpenPgmCReferencePgmTransportSendPacketv.md'>pgm_transport_send_packetv()</a>]</tt> with <tt><a href='OpenPgm2CReferencePgmSendSkbv.md'>pgm_send_skbv()</a></tt>.
  * Zero-copy receive processing needs to be updated to handle PGM skbuffs instead of data pointers.


### Details ###
Send and receive zero-copy handling has been consolidating into to using new PGM variants of Linux SKBs.  Memory is managed system wide by the GLib slab allocator instead of per transport.

**Example zero-copy send for version 1.0.**
```
 const char src[] = "i am not a string";
 gpointer data;
 gsize bytes_sent;
 struct iovec vector[1];
 
 data = pgm_packetv_alloc (transport, FALSE);
 memcpy (data, src, sizeof(src));
 vector[0].iov_base = data;
 vector[0].iov_len = sizeof(src);
 bytes_sent = pgm_transport_send_packetv (transport, vector, G_N_ELEMENTS(vector), 0, TRUE);
```
**Updated zero-copy send example for version 2.0.**
```
 const char src[] = "i am not a string";
 gsize bytes_sent;
 struct pgm_sk_buff_t* skb;
 
 skb = pgm_alloc_skb (1500);
 memcpy (pgm_skb_put (skb, sizeof(src)), src, sizeof(src));
 pgm_send_skbv (transport, &skb, 1, &bytes_sent, TRUE);
```
**Example zero-copy receive for version 1.0.**
```
 pgm_msgv_t msgv[10], *pmsgv = msgv;
 gsize len;
 
 len = pgm_transport_recvmsgv (transport, msgv, G_N_ELEMENTS(msgv), 0);
 while (len) {
   g_message ("recv: %s from %d",
              pmsgv->msgv_iov->iov_base,
              pgm_tsi_print (pmsgv->msgv_tsi));
   pmsgv++;
 }
```
**Update zero-copy receive example for version 2.0.**
```
 pgm_msgv_t msgv[10], *pmsgv = msgv;
 gsize len = 0;
 
 pgm_recvmsgv (transport, msgv, G_N_ELEMENTS(msgv), 0, &len, NULL);
 while (len) {
   g_message ("recv: %s from %d",
              pmsgv->msgv_skb[0]->data,
              pgm_tsi_print (pmsgv->msgv_skb[0]->tsi));
   pmsgv++;
 }
```

## Monitoring and Administration ##
### Summary ###
  * Update calls to <tt><a href='OpenPgm2CReferencePgmHttpInit.md'>pgm_http_init()</a></tt> with optional <tt><a href='http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html#GError'>GError</a></tt> parameter.
  * Update calls to <tt><a href='OpenPgm2CReferencePgmSnmpInit.md'>pgm_snmp_init()</a></tt> with optional <tt><a href='http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html#GError'>GError</a></tt> parameter.


### Details ###
Version 2.0 introduces more detailed error reporting on network operations so asynchronous creation and receive calls include optional <tt><a href='http://library.gnome.org/devel/glib/stable/glib-Error-Reporting.html#GError'>GError</a></tt> return values.


**Version 1.0 code example.**
```
 if (!pgm_http_init (PGM_HTTP_DEFAULT_SERVER_PORT)) {
   g_error ("Unable to start HTTP interface.");
   pgm_shutdown ();
   return EXIT_FAILURE;
  }
  if (!pgm_snmp_init ()) {
    g_error ("Unable to start SNMP interface.");
    pgm_http_shutdown ();
    pgm_shutdown ();
    return EXIT_FAILURE;
  }
```
**Updated example for version 2.0.**
```
 GError *err = NULL;
 if (!pgm_http_init (PGM_HTTP_DEFAULT_SERVER_PORT, &err)) {
   g_error ("Unable to start HTTP interface: %s", err->message);
   g_error_free (err);
   pgm_shutdown ();
   return EXIT_FAILURE;
  }
  if (!pgm_snmp_init (&err)) {
    g_error ("Unable to start SNMP interface: %s", err->message);
    g_error_free (err);
    pgm_http_shutdown ();
    pgm_shutdown ();
    return EXIT_FAILURE;
  }
```