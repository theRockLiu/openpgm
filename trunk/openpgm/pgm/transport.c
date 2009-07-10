/* vim:ts=8:sts=8:sw=4:noai:noexpandtab
 *
 * PGM transport: manage incoming & outgoing sockets with ambient SPMs, 
 * transmit & receive windows.
 *
 * Copyright (c) 2006-2009 Miru Limited.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#ifdef CONFIG_HAVE_POLL
#	include <poll.h>
#endif
#ifdef CONFIG_HAVE_EPOLL
#	include <sys/epoll.h>
#endif
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>

#include <glib.h>

#include "pgm/transport.h"
#include "pgm/source.h"
#include "pgm/receiver.h"
#include "pgm/if.h"
#include "pgm/getnodeaddr.h"
#include "pgm/indextoaddr.h"
#include "pgm/ip.h"
#include "pgm/packet.h"
#include "pgm/net.h"
#include "pgm/txwi.h"
#include "pgm/rxwi.h"
#include "pgm/rate_control.h"
#include "pgm/sn.h"
#include "pgm/time.h"
#include "pgm/timer.h"
#include "pgm/checksum.h"
#include "pgm/reed_solomon.h"
#include "pgm/err.h"

#define TRANSPORT_DEBUG
//#define TRANSPORT_SPM_DEBUG

#ifndef TRANSPORT_DEBUG
#	define g_trace(m,...)		while (0)
#else
#include <ctype.h>
#	ifdef TRANSPORT_SPM_DEBUG
#		define g_trace(m,...)		g_debug(__VA_ARGS__)
#	else
#		define g_trace(m,...)		do { if (strcmp((m),"SPM")) { g_debug(__VA_ARGS__); } } while (0)
#	endif
#endif


/* global locals */
GStaticRWLock pgm_transport_list_lock = G_STATIC_RW_LOCK_INIT;		/* list of all transports for admin interfaces */
GSList* pgm_transport_list = NULL;


static gboolean on_timer_notify (GIOChannel*, GIOCondition, gpointer);
static gboolean on_timer_shutdown (GIOChannel*, GIOCondition, gpointer);
static gboolean g_source_remove_context (GMainContext*, guint);


/* re-entrant form of pgm_print_tsi()
 */
int
pgm_print_tsi_r (
	const pgm_tsi_t*	tsi,
	char*			buf,
	gsize			bufsize
	)
{
	g_return_val_if_fail (tsi != NULL, -EINVAL);
	g_return_val_if_fail (buf != NULL, -EINVAL);

	const guint8* gsi = (const guint8*)tsi;
	guint16 source_port = tsi->sport;
	snprintf(buf, bufsize, "%i.%i.%i.%i.%i.%i.%i",
		gsi[0], gsi[1], gsi[2], gsi[3], gsi[4], gsi[5], g_ntohs (source_port));
	return 0;
}

/* transform TSI to ASCII string form.
 *
 * on success, returns pointer to ASCII string.  on error, returns NULL.
 */
gchar*
pgm_print_tsi (
	const pgm_tsi_t*	tsi
	)
{
	g_return_val_if_fail (tsi != NULL, NULL);

	static char buf[PGM_TSISTRLEN];
	pgm_print_tsi_r (tsi, buf, sizeof(buf));
	return buf;
}

/* create hash value of TSI for use with GLib hash tables.
 *
 * on success, returns a hash value corresponding to the TSI.  on error, fails
 * on assert.
 */
guint
pgm_tsi_hash (
	gconstpointer v
        )
{
	g_assert( v != NULL );
	const pgm_tsi_t* tsi = v;
	char buf[PGM_TSISTRLEN];
	int valid = pgm_print_tsi_r(tsi, buf, sizeof(buf));
	g_assert( valid == 0 );
	return g_str_hash( buf );
}

/* compare two transport session identifier TSI values.
 *
 * returns TRUE if they are equal, FALSE if they are not.
 */
gboolean
pgm_tsi_equal (
	gconstpointer   v,
	gconstpointer   v2
        )
{
	return memcmp (v, v2, sizeof(struct pgm_tsi_t)) == 0;
}

gsize
pgm_transport_pkt_offset (
	gboolean		can_fragment
	)
{
	return can_fragment ? ( sizeof(struct pgm_header)
			      + sizeof(struct pgm_data)
			      + sizeof(struct pgm_opt_length)
	                      + sizeof(struct pgm_opt_header)
			      + sizeof(struct pgm_opt_fragment) )
			    : ( sizeof(struct pgm_header) + sizeof(struct pgm_data) );
}

/* fast log base 2 of power of 2
 */

guint
pgm_power2_log2 (
	guint		v
	)
{
	static const unsigned int b[] = {0xAAAAAAAA, 0xCCCCCCCC, 0xF0F0F0F0, 0xFF00FF00, 0xFFFF0000};
	unsigned int r = (v & b[0]) != 0;
	for (int i = 4; i > 0; i--) {
		r |= ((v & b[i]) != 0) << i;
	}
	return r;
}

/* destroy a pgm_transport object and contents, if last transport also destroy
 * associated event loop
 *
 * TODO: clear up locks on destruction: 1: flushing, 2: destroying:, 3: destroyed.
 *
 * If application calls a function on the transport after destroy() it is a
 * programmer error: segv likely to occur on unlock.
 *
 * on success, returns 0.  if transport is invalid, or previously destroyed,
 * returns -EINVAL.
 */

int
pgm_transport_destroy (
	pgm_transport_t*	transport,
	gboolean		flush
	)
{
	g_return_val_if_fail (transport != NULL, -EINVAL);

	g_static_rw_lock_writer_lock (&pgm_transport_list_lock);
	pgm_transport_list = g_slist_remove (pgm_transport_list, transport);
	g_static_rw_lock_writer_unlock (&pgm_transport_list_lock);

	transport->is_open = FALSE;

/* rollback any pkt_dontwait APDU */
	if (transport->is_apdu_eagain)
	{
		((pgm_txw_t*)transport->txw)->lead = transport->pkt_dontwait_state.first_sqn - 1;
		g_static_rw_lock_writer_unlock (&transport->txw_lock);
		transport->is_apdu_eagain = FALSE;
	}

/* cleanup rdata-transmit channel in timer thread */
	if (transport->rdata_id > 0) {
		g_source_remove_context (transport->timer_context, transport->rdata_id);
	}
	if (transport->rdata_channel) {
		g_io_channel_unref (transport->rdata_channel);
	}

/* terminate & join internal thread */
	if (transport->notify_id > 0) {
		g_source_remove_context (transport->timer_context, transport->notify_id);
	}
	if (transport->notify_channel) {
		g_io_channel_unref (transport->notify_channel);
	}

	if (transport->timer_id > 0) {
		g_source_remove_context (transport->timer_context, transport->timer_id);
	}

#ifndef PGM_SINGLE_THREAD
	if (transport->timer_thread) {
		pgm_notify_send (&transport->timer_shutdown);
		g_thread_join (transport->timer_thread);
		transport->timer_thread = NULL;
	}
#endif /* !PGM_SINGLE_THREAD */

/* cleanup shutdown comms */
	if (transport->shutdown_channel) {
		g_io_channel_unref (transport->shutdown_channel);
	}

	g_static_mutex_lock (&transport->mutex);

/* assume lock from create() if not bound */
	if (transport->is_bound) {
		g_static_mutex_lock (&transport->send_mutex);
		g_static_mutex_lock (&transport->send_with_router_alert_mutex);
	}

/* flush data by sending heartbeat SPMs & processing NAKs until ambient */
	if (flush) {
	}

	if (transport->peers_hashtable) {
		g_hash_table_destroy (transport->peers_hashtable);
		transport->peers_hashtable = NULL;
	}
	if (transport->peers_list) {
		g_trace ("INFO","destroying peer data.");

		do {
			GList* next = transport->peers_list->next;
			_pgm_peer_unref ((pgm_peer_t*)transport->peers_list->data);

			transport->peers_list = next;
		} while (transport->peers_list);
	}

	if (transport->txw) {
		g_trace ("INFO","destroying transmit window.");
		pgm_txw_shutdown (transport->txw);
		transport->txw = NULL;
	}

	if (transport->rate_control) {
		g_trace ("INFO","destroying rate control.");
		_pgm_rate_destroy (transport->rate_control);
		transport->rate_control = NULL;
	}
	if (transport->recv_sock) {
		g_trace ("INFO","closing receive socket.");
		close(transport->recv_sock);
		transport->recv_sock = 0;
	}

	if (transport->send_sock) {
		g_trace ("INFO","closing send socket.");
		close(transport->send_sock);
		transport->send_sock = 0;
	}
	if (transport->send_with_router_alert_sock) {
		g_trace ("INFO","closing send with router alert socket.");
		close(transport->send_with_router_alert_sock);
		transport->send_with_router_alert_sock = 0;
	}

	if (transport->spm_heartbeat_interval) {
		g_free (transport->spm_heartbeat_interval);
		transport->spm_heartbeat_interval = NULL;
	}

	if (transport->rand_) {
		g_rand_free (transport->rand_);
		transport->rand_ = NULL;
	}

	pgm_notify_destroy (&transport->rdata_notify);
	pgm_notify_destroy (&transport->timer_notify);
	pgm_notify_destroy (&transport->waiting_notify);

	g_static_rw_lock_free (&transport->peers_lock);

	g_static_rw_lock_free (&transport->txw_lock);

	if (transport->is_bound) {
		g_static_mutex_unlock (&transport->send_mutex);
		g_static_mutex_unlock (&transport->send_with_router_alert_mutex);
	}
	g_static_mutex_free (&transport->send_mutex);
	g_static_mutex_free (&transport->send_with_router_alert_mutex);

	g_static_mutex_unlock (&transport->mutex);
	g_static_mutex_free (&transport->mutex);

	if (transport->spm_packet) {
		g_slice_free1 (transport->spm_len, transport->spm_packet);
		transport->spm_packet = NULL;
	}

	if (transport->parity_buffer) {
		pgm_free_skb (transport->parity_buffer);
		transport->parity_buffer = NULL;
	}
	if (transport->rs) {
		_pgm_rs_destroy (transport->rs);
		transport->rs = NULL;
	}

	if (transport->rx_buffer) {
		pgm_free_skb (transport->rx_buffer);
		transport->rx_buffer = NULL;
	}

	g_free (transport);

	g_trace ("INFO","finished.");
	return 0;
}

/* create a pgm_transport object.  create sockets that require superuser priviledges, if this is
 * the first instance also create a real-time priority receiving thread.  if interface ports
 * are specified then UDP encapsulation will be used instead of raw protocol.
 *
 * if send == recv only two sockets need to be created iff ip headers are not required (IPv6).
 *
 * all receiver addresses must be the same family.
 * interface and multiaddr must be the same family.
 * family cannot be AF_UNSPEC!
 *
 * returns 0 on success, or -1 on error and sets errno appropriately.
 */

#if ( AF_INET != PF_INET ) || ( AF_INET6 != PF_INET6 )
#error AF_INET and PF_INET are different values, the bananas are jumping in their pyjamas!
#endif

int
pgm_transport_create (
	pgm_transport_t**		transport_,
	pgm_gsi_t*			gsi,
	guint16				sport,		/* set to 0 to randomly select */
	guint16				dport,
	struct group_source_req*	recv_gsr,	/* receive port, multicast group & interface address */
	gsize				recv_len,
	struct group_source_req*	send_gsr	/* send ... */
	)
{
	g_return_val_if_fail (transport_ != NULL, -EINVAL);
	g_return_val_if_fail (gsi != NULL, -EINVAL);
	if (sport) g_return_val_if_fail (sport != dport, -EINVAL);
	g_return_val_if_fail (recv_gsr != NULL, -EINVAL);
	g_return_val_if_fail (recv_len > 0, -EINVAL);
	g_return_val_if_fail (recv_len <= IP_MAX_MEMBERSHIPS, -EINVAL);
	g_return_val_if_fail (send_gsr != NULL, -EINVAL);
	for (unsigned i = 0; i < recv_len; i++)
	{
		g_return_val_if_fail (pgm_sockaddr_family(&recv_gsr[i].gsr_group) == pgm_sockaddr_family(&recv_gsr[0].gsr_group), -EINVAL);
		g_return_val_if_fail (pgm_sockaddr_family(&recv_gsr[i].gsr_group) == pgm_sockaddr_family(&recv_gsr[i].gsr_source), -EINVAL);
	}
	g_return_val_if_fail (pgm_sockaddr_family(&send_gsr->gsr_group) == pgm_sockaddr_family(&send_gsr->gsr_source), -EINVAL);

	int retval = 0;
	pgm_transport_t* transport;

/* create transport object */
	transport = g_malloc0 (sizeof(pgm_transport_t));

/* transport defaults */
	transport->can_send_data = TRUE;
	transport->can_send_nak  = TRUE;
	transport->can_recv = TRUE;

/* regular send lock */
	g_static_mutex_init (&transport->send_mutex);

/* IP router alert send lock */
	g_static_mutex_init (&transport->send_with_router_alert_mutex);

/* timer lock */
	g_static_mutex_init (&transport->mutex);

/* transmit window read/write lock */
	g_static_rw_lock_init (&transport->txw_lock);

/* peer hash map & list lock */
	g_static_rw_lock_init (&transport->peers_lock);

/* lock tx until bound */
	g_static_mutex_lock (&transport->send_mutex);

	memcpy (&transport->tsi.gsi, gsi, 6);
	transport->dport = g_htons (dport);
	if (sport) {
		transport->tsi.sport = g_htons (sport);
	} else {
		do {
			transport->tsi.sport = g_htons (g_random_int_range (0, UINT16_MAX));
		} while (transport->tsi.sport == transport->dport);
	}

/* network data ports */
	transport->udp_encap_port = ((struct sockaddr_in*)&send_gsr->gsr_group)->sin_port;

/* copy network parameters */
	memcpy (&transport->send_gsr, send_gsr, sizeof(struct group_source_req));
	for (unsigned i = 0; i < recv_len; i++)
	{
		memcpy (&transport->recv_gsr[i], &recv_gsr[i], sizeof(struct group_source_req));
	}
	transport->recv_gsr_len = recv_len;

/* open sockets to implement PGM */
	int socket_type, protocol;
	if (transport->udp_encap_port) {
		g_trace ("INFO", "opening UDP encapsulated sockets.");
		socket_type = SOCK_DGRAM;
		protocol = IPPROTO_UDP;
	} else {
		g_trace ("INFO", "opening raw sockets.");
		socket_type = SOCK_RAW;
		protocol = ipproto_pgm;
	}

	if ((transport->recv_sock = socket(pgm_sockaddr_family(&recv_gsr[0].gsr_group),
						socket_type,
						protocol)) < 0)
	{
		retval = transport->recv_sock;
		if (retval == EPERM && 0 != getuid()) {
			g_critical ("PGM protocol requires this program to run as superuser.");
		}
		goto err_destroy;
	}

	if ((transport->send_sock = socket(pgm_sockaddr_family(&send_gsr->gsr_group),
						socket_type,
						protocol)) < 0)
	{
		retval = transport->send_sock;
		goto err_destroy;
	}

	if ((transport->send_with_router_alert_sock = socket(pgm_sockaddr_family(&send_gsr->gsr_group),
						socket_type,
						protocol)) < 0)
	{
		retval = transport->send_with_router_alert_sock;
		goto err_destroy;
	}

/* create timer thread */
#ifndef PGM_SINGLE_THREAD
	GError* err;
	GThread* thread;

/* set up condition for thread context & loop being ready */
	transport->thread_mutex = g_mutex_new ();
	transport->thread_cond = g_cond_new ();

	thread = g_thread_create_full (pgm_timer_thread,
					transport,
					0,
					TRUE,
					TRUE,
					G_THREAD_PRIORITY_HIGH,
					&err);
	if (thread) {
		transport->timer_thread = thread;
	} else {
		g_error ("thread failed: %i %s", err->code, err->message);
		goto err_destroy;
	}

	g_mutex_lock (transport->thread_mutex);
	while (!transport->timer_loop)
		g_cond_wait (transport->thread_cond, transport->thread_mutex);
	g_mutex_unlock (transport->thread_mutex);

	g_mutex_free (transport->thread_mutex);
	transport->thread_mutex = NULL;
	g_cond_free (transport->thread_cond);
	transport->thread_cond = NULL;

#endif /* !PGM_SINGLE_THREAD */

	*transport_ = transport;

	g_static_rw_lock_writer_lock (&pgm_transport_list_lock);
	pgm_transport_list = g_slist_append (pgm_transport_list, transport);
	g_static_rw_lock_writer_unlock (&pgm_transport_list_lock);

	return retval;

err_destroy:
	if (transport->thread_mutex) {
		g_mutex_free (transport->thread_mutex);
		transport->thread_mutex = NULL;
	}
	if (transport->thread_cond) {
		g_cond_free (transport->thread_cond);
		transport->thread_cond = NULL;
	}
	if (transport->timer_thread) {
	}
		
	if (transport->recv_sock) {
		close(transport->recv_sock);
		transport->recv_sock = 0;
	}
	if (transport->send_sock) {
		close(transport->send_sock);
		transport->send_sock = 0;
	}
	if (transport->send_with_router_alert_sock) {
		close(transport->send_with_router_alert_sock);
		transport->send_with_router_alert_sock = 0;
	}

	g_static_mutex_free (&transport->mutex);
	g_free (transport);
	transport = NULL;

	return retval;
}

/* helper to drop out of setuid 0 after creating PGM sockets
 */
void
pgm_drop_superuser (void)
{
	if (0 == getuid()) {
		setuid((gid_t)65534);
		setgid((uid_t)65534);
	}
}

/* 0 < tpdu < 65536 by data type (guint16)
 *
 * IPv4:   68 <= tpdu < 65536		(RFC 2765)
 * IPv6: 1280 <= tpdu < 65536		(RFC 2460)
 *
 * on success, returns 0.  on invalid setting, returns -EINVAL.
 */

int
pgm_transport_set_max_tpdu (
	pgm_transport_t*	transport,
	guint16			max_tpdu
	)
{
	g_return_val_if_fail (transport != NULL, -EINVAL);
	g_return_val_if_fail (!transport->is_bound, -EINVAL);
	g_return_val_if_fail (max_tpdu >= (sizeof(struct pgm_ip) + sizeof(struct pgm_header)), -EINVAL);

	g_static_mutex_lock (&transport->mutex);
	transport->max_tpdu = max_tpdu;
	g_static_mutex_unlock (&transport->mutex);

	return 0;
}

/* TRUE = enable multicast loopback and for UDP encapsulation SO_REUSEADDR,
 * FALSE = default, to disable.
 *
 * on success, returns 0.  on invalid setting, returns -EINVAL.
 */

int
pgm_transport_set_multicast_loop (
	pgm_transport_t*	transport,
	gboolean		use_multicast_loop
	)
{
	g_return_val_if_fail (transport != NULL, -EINVAL);
	g_return_val_if_fail (!transport->is_bound, -EINVAL);

	g_static_mutex_lock (&transport->mutex);
	transport->use_multicast_loop = use_multicast_loop;
	g_static_mutex_unlock (&transport->mutex);

	return 0;
}

/* 0 < hops < 256, hops == -1 use kernel default (ignored).
 *
 * on success, returns 0.  on invalid setting, returns -EINVAL.
 */

int
pgm_transport_set_hops (
	pgm_transport_t*	transport,
	gint			hops
	)
{
	g_return_val_if_fail (transport != NULL, -EINVAL);
	g_return_val_if_fail (!transport->is_bound, -EINVAL);
	g_return_val_if_fail (hops > 0, -EINVAL);
	g_return_val_if_fail (hops < 256, -EINVAL);

	g_static_mutex_lock (&transport->mutex);
	transport->hops = hops;
	g_static_mutex_unlock (&transport->mutex);

	return 0;
}

/* 0 < wmem < wmem_max (user)
 *
 * operating system and sysctl dependent maximum, minimum on Linux 256 (doubled).
 *
 * on success, returns 0.  on invalid setting, returns -EINVAL.
 */

int
pgm_transport_set_sndbuf (
	pgm_transport_t*	transport,
	int			size		/* not gsize/gssize as we propogate to setsockopt() */
	)
{
	g_return_val_if_fail (transport != NULL, -EINVAL);
	g_return_val_if_fail (!transport->is_bound, -EINVAL);
	g_return_val_if_fail (size > 0, -EINVAL);

	int wmem_max;
	FILE *fp = fopen ("/proc/sys/net/core/wmem_max", "r");
	if (fp) {
		fscanf (fp, "%d", &wmem_max);
		fclose (fp);
		g_return_val_if_fail (size <= wmem_max, -EINVAL);
	} else {
		g_warning ("cannot open /proc/sys/net/core/wmem_max");
	}

	g_static_mutex_lock (&transport->mutex);
	transport->sndbuf = size;
	g_static_mutex_unlock (&transport->mutex);

	return 0;
}

/* 0 < rmem < rmem_max (user)
 *
 * minimum on Linux is 2048 (doubled).
 *
 * on success, returns 0.  on invalid setting, returns -EINVAL.
 */

int
pgm_transport_set_rcvbuf (
	pgm_transport_t*	transport,
	int			size		/* not gsize/gssize */
	)
{
	g_return_val_if_fail (transport != NULL, -EINVAL);
	g_return_val_if_fail (!transport->is_bound, -EINVAL);
	g_return_val_if_fail (size > 0, -EINVAL);

	int rmem_max;
	FILE *fp = fopen ("/proc/sys/net/core/rmem_max", "r");
	if (fp) {
		fscanf (fp, "%d", &rmem_max);
		fclose (fp);
		g_return_val_if_fail (size <= rmem_max, -EINVAL);
	} else {
		g_warning ("cannot open /proc/sys/net/core/rmem_max");
	}

	g_static_mutex_lock (&transport->mutex);
	transport->rcvbuf = size;
	g_static_mutex_unlock (&transport->mutex);

	return 0;
}

/* context aware g_io helpers
 *
 * on success, returns id of GSource.
 */
static guint
g_io_add_watch_context_full (
	GIOChannel*		channel,
	GMainContext*		context,
	gint			priority,
	GIOCondition		condition,
	GIOFunc			function,
	gpointer		user_data,
	GDestroyNotify		notify
	)
{
	GSource *source;
	guint id;
  
	g_return_val_if_fail (channel != NULL, 0);

	source = g_io_create_watch (channel, condition);

	if (priority != G_PRIORITY_DEFAULT)
		g_source_set_priority (source, priority);
	g_source_set_callback (source, (GSourceFunc)function, user_data, notify);

	id = g_source_attach (source, context);
	g_source_unref (source);

	return id;
}

static gboolean
g_source_remove_context (
	GMainContext*		context,
	guint			tag
	)
{
	GSource* source;

	g_return_val_if_fail (tag > 0, FALSE);

	source = g_main_context_find_source_by_id (context, tag);
	if (source)
		g_source_destroy (source);

	return source != NULL;
}

/* bind the sockets to the link layer to start receiving data.
 *
 * returns 0 on success, or -1 on error and sets errno appropriately,
 *			 or -2 on NS lookup error and sets h_errno appropriately.
 */

int
pgm_transport_bind (
	pgm_transport_t*	transport
	)
{
	g_return_val_if_fail (transport != NULL, -EINVAL);
	g_return_val_if_fail (!transport->is_bound, -EINVAL);

	int retval = 0;

	g_static_mutex_lock (&transport->mutex);

	g_trace ("INFO","creating new random number generator.");
	transport->rand_ = g_rand_new();

	if (transport->can_send_data)
	{
		g_trace ("INFO","create rx to nak processor notify channel.");
		retval = pgm_notify_init (&transport->rdata_notify);
		if (retval < 0) {
			g_static_mutex_unlock (&transport->mutex);
			goto out;
		}
	}

	g_trace ("INFO","create any to timer notify channel.");
	retval = pgm_notify_init (&transport->timer_notify);
	if (retval < 0) {
		g_static_mutex_unlock (&transport->mutex);
		goto out;
	}

	g_trace ("INFO","create timer shutdown channel.");
	retval = pgm_notify_init (&transport->timer_shutdown);
	if (retval < 0) {
		g_static_mutex_unlock (&transport->mutex);
		goto out;
	}

	if (transport->can_recv)
	{
		g_trace ("INFO","create waiting notify channel.");
		retval = pgm_notify_init (&transport->waiting_notify);
		if (retval < 0) {
			g_static_mutex_unlock (&transport->mutex);
			goto out;
		}
	}

/* determine IP header size for rate regulation engine & stats */
	switch (pgm_sockaddr_family(&transport->send_gsr.gsr_group)) {
	case AF_INET:
		transport->iphdr_len = sizeof(struct pgm_ip);
		break;

	case AF_INET6:
		transport->iphdr_len = sizeof(struct pgm_ip6_hdr);
		break;
	}
	g_trace ("INFO","assuming IP header size of %" G_GSIZE_FORMAT " bytes", transport->iphdr_len);

	if (transport->udp_encap_port)
	{
		guint udphdr_len = sizeof( struct pgm_udphdr );
		g_trace ("INFO","assuming UDP header size of %i bytes", udphdr_len);
		transport->iphdr_len += udphdr_len;
	}

	transport->max_tsdu = transport->max_tpdu - transport->iphdr_len - pgm_transport_pkt_offset (FALSE);
	transport->max_tsdu_fragment = transport->max_tpdu - transport->iphdr_len - pgm_transport_pkt_offset (TRUE);

	if (transport->can_send_data)
	{
		g_trace ("INFO","construct transmit window.");
		transport->txw = transport->txw_sqns ?
					pgm_txw_init (0, transport->txw_sqns, 0, 0) :
					pgm_txw_init (transport->max_tpdu, 0, transport->txw_secs, transport->txw_max_rte);
	}

/* create peer list */
	if (transport->can_recv)
	{
		transport->peers_hashtable = g_hash_table_new (pgm_tsi_hash, pgm_tsi_equal);
	}

	if (transport->udp_encap_port)
	{
/* set socket sharing if loopback enabled, needs to be performed pre-bind */
		if (transport->use_multicast_loop)
		{
			g_trace ("INFO","set socket sharing.");
			gboolean v = TRUE;
			retval = setsockopt(transport->recv_sock, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
			if (retval < 0) {
				g_static_mutex_unlock (&transport->mutex);
				goto out;
			}
			retval = setsockopt(transport->send_sock, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
			if (retval < 0) {
				g_static_mutex_unlock (&transport->mutex);
				goto out;
			}
			retval = setsockopt(transport->send_with_router_alert_sock, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));
			if (retval < 0) {
				g_static_mutex_unlock (&transport->mutex);
				goto out;
			}
		}

/* request extra packet information to determine destination address on each packet */
		g_trace ("INFO","request socket packet-info.");
		int recv_family = pgm_sockaddr_family(&transport->recv_gsr[0].gsr_group);
		retval = pgm_sockaddr_pktinfo (transport->recv_sock, recv_family, TRUE);
		if (retval < 0) {
			g_static_mutex_unlock (&transport->mutex);
			goto out;
		}
	}
	else
	{
		int recv_family = pgm_sockaddr_family(&transport->recv_gsr[0].gsr_group);
		if (AF_INET == recv_family)
		{
/* include IP header only for incoming data, only works for IPv4 */
			g_trace ("INFO","request IP headers.");
			retval = pgm_sockaddr_hdrincl (transport->recv_sock, recv_family, TRUE);
			if (retval < 0) {
				g_static_mutex_unlock (&transport->mutex);
				goto out;
			}
		}
		else
		{
			g_assert (AF_INET6 == recv_family);
			g_trace ("INFO","request socket packet-info.");
			retval = pgm_sockaddr_pktinfo (transport->recv_sock, recv_family, TRUE);
			if (retval < 0) {
				g_static_mutex_unlock (&transport->mutex);
				goto out;
			}
		}
	}

/* buffers, set size first then re-read to confirm actual value */
	if (transport->rcvbuf)
	{
		g_trace ("INFO","set receive socket buffer size.");
		retval = setsockopt(transport->recv_sock, SOL_SOCKET, SO_RCVBUF, (char*)&transport->rcvbuf, sizeof(transport->rcvbuf));
		if (retval < 0) {
			g_static_mutex_unlock (&transport->mutex);
			goto out;
		}
	}
	if (transport->sndbuf)
	{
		g_trace ("INFO","set send socket buffer size.");
		retval = setsockopt(transport->send_sock, SOL_SOCKET, SO_SNDBUF, (char*)&transport->sndbuf, sizeof(transport->sndbuf));
		if (retval < 0) {
			g_static_mutex_unlock (&transport->mutex);
			goto out;
		}
		retval = setsockopt(transport->send_with_router_alert_sock, SOL_SOCKET, SO_SNDBUF, (char*)&transport->sndbuf, sizeof(transport->sndbuf));
		if (retval < 0) {
			g_static_mutex_unlock (&transport->mutex);
			goto out;
		}
	}

/* Most socket-level options utilize an int parameter for optval. */
	int buffer_size;
	socklen_t len = sizeof(buffer_size);
	retval = getsockopt(transport->recv_sock, SOL_SOCKET, SO_RCVBUF, &buffer_size, &len);
	if (retval < 0) {
		g_static_mutex_unlock (&transport->mutex);
		goto out;
	}
	g_trace ("INFO","receive buffer set at %i bytes.", buffer_size);

	retval = getsockopt(transport->send_sock, SOL_SOCKET, SO_SNDBUF, &buffer_size, &len);
	if (retval < 0) {
		g_static_mutex_unlock (&transport->mutex);
		goto out;
	}
	retval = getsockopt(transport->send_with_router_alert_sock, SOL_SOCKET, SO_SNDBUF, &buffer_size, &len);
	if (retval < 0) {
		g_static_mutex_unlock (&transport->mutex);
		goto out;
	}
	g_trace ("INFO","send buffer set at %i bytes.", buffer_size);

/* bind udp unicast sockets to interfaces, note multicast on a bound interface is
 * fruity on some platforms so callee should specify any interface.
 *
 * after binding default interfaces (0.0.0.0) are resolved
 */
/* TODO: different ports requires a new bound socket */

	struct sockaddr_storage recv_addr;
	memset (&recv_addr, 0, sizeof(recv_addr));

#ifdef CONFIG_BIND_INADDR_ANY

/* force default interface for bind-only, source address is still valid for multicast membership */
	((struct sockaddr*)&recv_addr)->sa_family = pgm_sockaddr_family(&transport->recv_gsr[0].gsr_group);
	switch (pgm_sockaddr_family(&recv_addr)) {
	case AF_INET:
		((struct sockaddr_in*)&recv_addr)->sin_addr.s_addr = INADDR_ANY;
		break;

	case AF_INET6:
		((struct sockaddr_in6*)&recv_addr)->sin6_addr = in6addr_any;
		break;
	}
#else
	retval = _pgm_if_indextoaddr (transport->recv_gsr[0].gsr_interface, pgm_sockaddr_family(&transport->recv_gsr[0].gsr_group), pgm_sockaddr_scope_id(&transport->recv_gsr[0].gsr_group), &recv_addr);
	if (retval < 0) {
#ifdef TRANSPORT_DEBUG
		int errno_ = errno;
		g_trace ("INFO","_pgm_if_indextoaddr failed on recv_gsr[0] interface %i, family %s, %s",
				transport->recv_gsr[0].gsr_interface,
				AF_INET == pgm_sockaddr_family(&transport->send_gsr.gsr_group) ? "AF_INET" :
					( AF_INET6 == pgm_sockaddr_family(&transport->send_gsr.gsr_group) ? "AF_INET6" :
					  "AF_UNKNOWN" ),
				strerror(errno_));
		errno = errno_;
#endif
		g_static_mutex_unlock (&transport->mutex);
		goto out;
	}
#ifdef TRANSPORT_DEBUG
	else
	{
		g_trace ("INFO","binding receive socket to interface index %i", transport->recv_gsr[0].gsr_interface);
	}
#endif

#endif /* CONFIG_BIND_INADDR_ANY */

	((struct sockaddr_in*)&recv_addr)->sin_port = ((struct sockaddr_in*)&transport->recv_gsr[0].gsr_group)->sin_port;

	retval = bind (transport->recv_sock,
			(struct sockaddr*)&recv_addr,
			pgm_sockaddr_len(&recv_addr));
	if (retval < 0) {
#ifdef TRANSPORT_DEBUG
		int errno_ = errno;
		char s[INET6_ADDRSTRLEN];
		pgm_sockaddr_ntop (&recv_addr, s, sizeof(s));
		g_trace ("INFO","bind failed on recv_gsr[0] interface \"%s\" %s", s, strerror(errno_));
		errno = errno_;
#endif
		g_static_mutex_unlock (&transport->mutex);
		goto out;
	}

#ifdef TRANSPORT_DEBUG
	{
		char s[INET6_ADDRSTRLEN];
		pgm_sockaddr_ntop (&recv_addr, s, sizeof(s));
		g_trace ("INFO","bind succeeded on recv_gsr[0] interface %s", s);
	}
#endif

/* keep a copy of the original address source to re-use for router alert bind */
	struct sockaddr_storage send_addr, send_with_router_alert_addr;
	memset (&send_addr, 0, sizeof(send_addr));

	retval = _pgm_if_indextoaddr (transport->send_gsr.gsr_interface, pgm_sockaddr_family(&transport->send_gsr.gsr_group), pgm_sockaddr_scope_id(&transport->send_gsr.gsr_group), (struct sockaddr*)&send_addr);
	if (retval < 0) {
#ifdef TRANSPORT_DEBUG
		int errno_ = errno;
		g_trace ("INFO","_pgm_if_indextoaddr failed on send_gsr interface %i, family %s, %s",
				transport->send_gsr.gsr_interface,
				AF_INET == pgm_sockaddr_family(&transport->send_gsr.gsr_group) ? "AF_INET" :
					( AF_INET6 == pgm_sockaddr_family(&transport->send_gsr.gsr_group) ? "AF_INET6" :
					  "AF_UNKNOWN" ),
				strerror(errno_));
		errno = errno_;
#endif
		g_static_mutex_unlock (&transport->mutex);
		goto out;
	}
#ifdef TRANSPORT_DEBUG
	else
	{
		g_trace ("INFO","binding send socket to interface index %i", transport->send_gsr.gsr_interface);
	}
#endif

	memcpy (&send_with_router_alert_addr, &send_addr, pgm_sockaddr_len(&send_addr));

	retval = bind (transport->send_sock,
			(struct sockaddr*)&send_addr,
			pgm_sockaddr_len(&send_addr));
	if (retval < 0) {
#ifdef TRANSPORT_DEBUG
		int errno_ = errno;
		char s[INET6_ADDRSTRLEN];
		pgm_sockaddr_ntop (&send_addr, s, sizeof(s));
		g_trace ("INFO","bind failed on send_gsr interface %s: %s", s, strerror(errno_));
		errno = errno_;
#endif
		g_static_mutex_unlock (&transport->mutex);
		goto out;
	}

/* resolve bound address if 0.0.0.0 */
	switch (pgm_sockaddr_family(&send_addr)) {
	case AF_INET:
		if (((struct sockaddr_in*)&send_addr)->sin_addr.s_addr == INADDR_ANY)
		{
			retval = _pgm_if_getnodeaddr (AF_INET, (struct sockaddr*)&send_addr, sizeof(send_addr));
			if (retval < 0) {
				g_static_mutex_unlock (&transport->mutex);
				goto out;
			}
		}
		break;

	case AF_INET6:
		if (memcmp (&in6addr_any, &((struct sockaddr_in6*)&send_addr)->sin6_addr, sizeof(in6addr_any)) == 0)
		{
			retval = _pgm_if_getnodeaddr (AF_INET6, (struct sockaddr*)&send_addr, sizeof(send_addr));
			if (retval < 0) {
				g_static_mutex_unlock (&transport->mutex);
				goto out;
			}
		}
		break;
	}

#ifdef TRANSPORT_DEBUG
	{
		char s[INET6_ADDRSTRLEN];
		pgm_sockaddr_ntop (&send_addr, s, sizeof(s));
		g_trace ("INFO","bind succeeded on send_gsr interface %s", s);
	}
#endif

	retval = bind (transport->send_with_router_alert_sock,
			(struct sockaddr*)&send_with_router_alert_addr,
			pgm_sockaddr_len(&send_with_router_alert_addr));
	if (retval < 0) {
#ifdef TRANSPORT_DEBUG
		int errno_ = errno;
		char s[INET6_ADDRSTRLEN];
		pgm_sockaddr_ntop (&send_with_router_alert_addr, s, sizeof(s));
		g_trace ("INFO","bind (router alert) failed on send_gsr interface %s: %s", s, strerror(errno_));
		errno = errno_;
#endif
		g_static_mutex_unlock (&transport->mutex);
		goto out;
	}

#ifdef TRANSPORT_DEBUG
	{
		char s[INET6_ADDRSTRLEN];
		pgm_sockaddr_ntop (&send_with_router_alert_addr, s, sizeof(s));
		g_trace ("INFO","bind (router alert) succeeded on send_gsr interface %s", s);
	}
#endif

/* save send side address for broadcasting as source nla */
	memcpy (&transport->send_addr, &send_addr, pgm_sockaddr_len(&send_addr));
	

/* receiving groups (multiple) */
	for (unsigned i = 0; i < transport->recv_gsr_len; i++)
	{
		struct group_source_req* p = &transport->recv_gsr[i];
		int recv_level = ( (AF_INET == pgm_sockaddr_family((&p->gsr_group))) ? SOL_IP : SOL_IPV6 );
		int optname = (pgm_sockaddr_cmp ((struct sockaddr*)&p->gsr_group, (struct sockaddr*)&p->gsr_source) == 0)
				? MCAST_JOIN_GROUP : MCAST_JOIN_SOURCE_GROUP;
		socklen_t plen = MCAST_JOIN_GROUP == optname ? sizeof(struct group_req) : sizeof(struct group_source_req);
		retval = setsockopt(transport->recv_sock, recv_level, optname, p, plen);
		if (retval < 0) {
#ifdef TRANSPORT_DEBUG
			int errno_ = errno;
			char s1[INET6_ADDRSTRLEN], s2[INET6_ADDRSTRLEN];
			pgm_sockaddr_ntop (&p->gsr_group, s1, sizeof(s1));
			pgm_sockaddr_ntop (&p->gsr_source, s2, sizeof(s2));
			if (optname == MCAST_JOIN_GROUP)
				g_trace ("INFO","MCAST_JOIN_GROUP failed on recv_gsr[%i] interface %i group %s: %s",
					i, p->gsr_interface, s1, strerror(errno_));
			else
				g_trace ("INFO","MCAST_JOIN_SOURCE_GROUP failed on recv_gsr[%i] interface %i group %s source %s: %s",
					i, p->gsr_interface, s1, s2, strerror(errno_));
			errno = errno_;
#endif
			g_static_mutex_unlock (&transport->mutex);
			goto out;
		}
#ifdef TRANSPORT_DEBUG
		else
		{
			char s1[INET6_ADDRSTRLEN], s2[INET6_ADDRSTRLEN];
			pgm_sockaddr_ntop (&p->gsr_group, s1, sizeof(s1));
			pgm_sockaddr_ntop (&p->gsr_source, s2, sizeof(s2));
			if (optname == MCAST_JOIN_GROUP)
				g_trace ("INFO","MCAST_JOIN_GROUP succeeded on recv_gsr[%i] interface %i group %s",
					i, p->gsr_interface, s1);
			else
				g_trace ("INFO","MCAST_JOIN_SOURCE_GROUP succeeded on recv_gsr[%i] interface %i group %s source %s",
					i, p->gsr_interface, s1, s2);
		}
#endif
	}

/* send group (singular) */
	retval = pgm_sockaddr_multicast_if (transport->send_sock, (struct sockaddr*)&transport->send_addr, transport->send_gsr.gsr_interface);
	if (retval < 0) {
#ifdef TRANSPORT_DEBUG
		int errno_ = errno;
		char s[INET6_ADDRSTRLEN];
		pgm_sockaddr_ntop (&transport->send_addr, s, sizeof(s));
		g_trace ("INFO","pgm_sockaddr_multicast_if failed on send_gsr address %s interface %i: %s",
					s, transport->send_gsr.gsr_interface, strerror(errno_));
		errno = errno_;
#endif
		g_static_mutex_unlock (&transport->mutex);
		goto out;
	}
#ifdef TRANSPORT_DEBUG
	else
	{
		char s[INET6_ADDRSTRLEN];
		pgm_sockaddr_ntop (&transport->send_addr, s, sizeof(s));
		g_trace ("INFO","pgm_sockaddr_multicast_if succeeded on send_gsr address %s interface %i",
					s, transport->send_gsr.gsr_interface);
	}
#endif
	retval = pgm_sockaddr_multicast_if (transport->send_with_router_alert_sock, (struct sockaddr*)&transport->send_addr, transport->send_gsr.gsr_interface);
	if (retval < 0) {
#ifdef TRANSPORT_DEBUG
		int errno_ = errno;
		char s[INET6_ADDRSTRLEN];
		pgm_sockaddr_ntop (&transport->send_addr, s, sizeof(s));
		g_trace ("INFO","pgm_sockaddr_multicast_if (router alert) failed on send_gsr address %s interface %i: %s",
					s, transport->send_gsr.gsr_interface, strerror(errno_));
		errno = errno_;
#endif
		g_static_mutex_unlock (&transport->mutex);
		goto out;
	}
#ifdef TRANSPORT_DEBUG
	else
	{
		char s[INET6_ADDRSTRLEN];
		pgm_sockaddr_ntop (&transport->send_addr, s, sizeof(s));
		g_trace ("INFO","pgm_sockaddr_multicast_if (router alert) succeeded on send_gsr address %s interface %i",
					s, transport->send_gsr.gsr_interface);
	}
#endif

/* multicast loopback */
	if (!transport->use_multicast_loop)
	{
		g_trace ("INFO","set multicast loop.");
		retval = pgm_sockaddr_multicast_loop (transport->recv_sock, pgm_sockaddr_family(&transport->recv_gsr[0].gsr_group), transport->use_multicast_loop);
		if (retval < 0) {
			g_static_mutex_unlock (&transport->mutex);
			goto out;
		}
		retval = pgm_sockaddr_multicast_loop (transport->send_sock, pgm_sockaddr_family(&transport->send_gsr.gsr_group), transport->use_multicast_loop);
		if (retval < 0) {
			g_static_mutex_unlock (&transport->mutex);
			goto out;
		}
		retval = pgm_sockaddr_multicast_loop (transport->send_with_router_alert_sock, pgm_sockaddr_family(&transport->send_gsr.gsr_group), transport->use_multicast_loop);
		if (retval < 0) {
			g_static_mutex_unlock (&transport->mutex);
			goto out;
		}
	}

/* multicast ttl: many crappy network devices go CPU ape with TTL=1, 16 is a popular alternative */
	g_trace ("INFO","set multicast hop limit.");
	retval = pgm_sockaddr_multicast_hops (transport->recv_sock, pgm_sockaddr_family(&transport->recv_gsr[0].gsr_group), transport->hops);
	if (retval < 0) {
		g_static_mutex_unlock (&transport->mutex);
		goto out;
	}
	retval = pgm_sockaddr_multicast_hops (transport->send_sock, pgm_sockaddr_family(&transport->send_gsr.gsr_group), transport->hops);
	if (retval < 0) {
		g_static_mutex_unlock (&transport->mutex);
		goto out;
	}
	retval = pgm_sockaddr_multicast_hops (transport->send_with_router_alert_sock, pgm_sockaddr_family(&transport->send_gsr.gsr_group), transport->hops);
	if (retval < 0) {
		g_static_mutex_unlock (&transport->mutex);
		goto out;
	}

/* set Expedited Forwarding PHB for network elements, no ECN.
 * 
 * codepoint 101110 (RFC 3246)
 */
	g_trace ("INFO","set packet differentiated services field to expedited forwarding.");
	int dscp = 0x2e << 2;
	retval = pgm_sockaddr_tos (transport->send_sock, pgm_sockaddr_family(&transport->send_gsr.gsr_group), dscp);
	if (retval < 0) {
		g_trace ("INFO","DSCP setting requires CAP_NET_ADMIN or ADMIN capability.");
		retval = 0;
		goto no_cap_net_admin;
	}
	retval = pgm_sockaddr_tos (transport->send_with_router_alert_sock, pgm_sockaddr_family(&transport->send_gsr.gsr_group), dscp);
	if (retval < 0) {
		g_static_mutex_unlock (&transport->mutex);
		goto out;
	}

no_cap_net_admin:

/* any to timer notify channel */
	transport->notify_channel = g_io_channel_unix_new (pgm_notify_get_fd (&transport->timer_notify));
	transport->notify_id = g_io_add_watch_context_full (transport->notify_channel, transport->timer_context, G_PRIORITY_HIGH, G_IO_IN, on_timer_notify, transport, NULL);

/* timer shutdown channel */
	transport->shutdown_channel = g_io_channel_unix_new (pgm_notify_get_fd (&transport->timer_shutdown));
	transport->shutdown_id = g_io_add_watch_context_full (transport->shutdown_channel, transport->timer_context, G_PRIORITY_HIGH, G_IO_IN, on_timer_shutdown, transport, NULL);

/* rx to nak processor notify channel */
	if (transport->can_send_data)
	{
		transport->rdata_channel = g_io_channel_unix_new (pgm_notify_get_fd (&transport->rdata_notify));
		transport->rdata_id = g_io_add_watch_context_full (transport->rdata_channel, transport->timer_context, G_PRIORITY_HIGH, G_IO_IN, _pgm_on_nak_notify, transport, NULL);

/* create recyclable SPM packet */
		switch (pgm_sockaddr_family(&transport->send_gsr.gsr_group)) {
		case AF_INET:
			transport->spm_len = sizeof(struct pgm_header) + sizeof(struct pgm_spm);
			break;

		case AF_INET6:
			transport->spm_len = sizeof(struct pgm_header) + sizeof(struct pgm_spm6);
			break;
		}

		if (transport->use_proactive_parity || transport->use_ondemand_parity)
		{
			transport->spm_len += sizeof(struct pgm_opt_length) +
					      sizeof(struct pgm_opt_header) +
					      sizeof(struct pgm_opt_parity_prm);
		}

		transport->spm_packet = g_slice_alloc0 (transport->spm_len);

		struct pgm_header* header = (struct pgm_header*)transport->spm_packet;
		struct pgm_spm* spm = (struct pgm_spm*)( header + 1 );
		memcpy (header->pgm_gsi, &transport->tsi.gsi, sizeof(pgm_gsi_t));
		header->pgm_sport	= transport->tsi.sport;
		header->pgm_dport	= transport->dport;
		header->pgm_type	= PGM_SPM;

		pgm_sockaddr_to_nla ((struct sockaddr*)&transport->send_addr, (char*)&spm->spm_nla_afi);

/* OPT_PARITY_PRM */
		if (transport->use_proactive_parity || transport->use_ondemand_parity)
		{
			header->pgm_options     = PGM_OPT_PRESENT | PGM_OPT_NETWORK;

			struct pgm_opt_length* opt_len = (struct pgm_opt_length*)(spm + 1);
			opt_len->opt_type	= PGM_OPT_LENGTH;
			opt_len->opt_length	= sizeof(struct pgm_opt_length);
			opt_len->opt_total_length = g_htons (	sizeof(struct pgm_opt_length) +
								sizeof(struct pgm_opt_header) +
								sizeof(struct pgm_opt_parity_prm) );
			struct pgm_opt_header* opt_header = (struct pgm_opt_header*)(opt_len + 1);
			opt_header->opt_type	= PGM_OPT_PARITY_PRM | PGM_OPT_END;
			opt_header->opt_length	= sizeof(struct pgm_opt_header) + sizeof(struct pgm_opt_parity_prm);
			struct pgm_opt_parity_prm* opt_parity_prm = (struct pgm_opt_parity_prm*)(opt_header + 1);
			opt_parity_prm->opt_reserved = (transport->use_proactive_parity ? PGM_PARITY_PRM_PRO : 0) |
						       (transport->use_ondemand_parity ? PGM_PARITY_PRM_OND : 0);
			opt_parity_prm->parity_prm_tgs = g_htonl (transport->rs_k);
		}

/* setup rate control */
		if (transport->txw_max_rte)
		{
			g_trace ("INFO","Setting rate regulation to %i bytes per second.",
					transport->txw_max_rte);
	
			retval = _pgm_rate_create (&transport->rate_control, transport->txw_max_rte, transport->iphdr_len);
			if (retval < 0) {
				g_static_mutex_unlock (&transport->mutex);
				goto out;
			}
		}

/* announce new transport by sending out SPMs */
		_pgm_send_spm_unlocked (transport);
		_pgm_send_spm_unlocked (transport);
		_pgm_send_spm_unlocked (transport);

/* parity buffer for odata/rdata transmission */
		if (transport->use_proactive_parity || transport->use_ondemand_parity)
		{
			g_trace ("INFO","Enabling Reed-Solomon forward error correction, RS(%i,%i).",
					transport->rs_n, transport->rs_k);
			transport->parity_buffer = pgm_alloc_skb (transport->max_tpdu);
			_pgm_rs_create (&transport->rs, transport->rs_n, transport->rs_k);
		}

		transport->next_poll = transport->next_ambient_spm = pgm_time_update_now() + transport->spm_ambient_interval;
	}
	else
	{
		g_assert (transport->can_recv);
		transport->next_poll = pgm_time_update_now() + pgm_secs( 30 );
	}

	g_trace ("INFO","adding dynamic timer");
	transport->timer_id = pgm_timer_add (transport);

/* allocate first incoming packet buffer */
	transport->rx_buffer = pgm_alloc_skb (transport->max_tpdu);

/* cleanup */
	transport->is_bound = TRUE;
	transport->is_open = TRUE;
	g_static_mutex_unlock (&transport->send_mutex);
	g_static_mutex_unlock (&transport->mutex);

	g_trace ("INFO","transport successfully created.");
out:
	return retval;
}

/* add select parameters for the transports receive socket(s)
 *
 * returns highest file descriptor used plus one.
 */

int
pgm_transport_select_info (
	pgm_transport_t*	transport,
	fd_set*			readfds,
	fd_set*			writefds,
	int*			n_fds		/* in: max fds, out: max (in:fds, transport:fds) */
	)
{
	g_assert (transport);
	g_assert (n_fds);

	if (!transport->is_open) {
		errno = EBADF;
		return -1;
	}

	int fds = 0;

	if (readfds)
	{
		FD_SET(transport->recv_sock, readfds);
		fds = transport->recv_sock + 1;

		if (transport->can_recv) {
			int waiting_fd = pgm_notify_get_fd (&transport->waiting_notify);
			FD_SET(waiting_fd, readfds);
			fds = MAX(fds, waiting_fd + 1);
		}
	}

	if (transport->can_send_data && writefds)
	{
		FD_SET(transport->send_sock, writefds);

		fds = MAX(transport->send_sock + 1, fds);
	}

	return *n_fds = MAX(fds, *n_fds);
}

#ifdef CONFIG_HAVE_POLL
/* add poll parameters for this transports receive socket(s)
 *
 * returns number of pollfd structures filled.
 */

int
pgm_transport_poll_info (
	pgm_transport_t*	transport,
	struct pollfd*		fds,
	int*			n_fds,		/* in: #fds, out: used #fds */
	int			events		/* POLLIN, POLLOUT */
	)
{
	g_assert (transport);
	g_assert (fds);
	g_assert (n_fds);

	if (!transport->is_open) {
		errno = EBADF;
		return -1;
	}

	int moo = 0;

/* we currently only support one incoming socket */
	if (events & POLLIN)
	{
		g_assert ( (1 + moo) <= *n_fds );
		fds[moo].fd = transport->recv_sock;
		fds[moo].events = POLLIN;
		moo++;
		if (transport->can_recv)
		{
			g_assert ( (1 + moo) <= *n_fds );
			fds[moo].fd = pgm_notify_get_fd (&transport->waiting_notify);
			fds[moo].events = POLLIN;
			moo++;
		}
	}

/* ODATA only published on regular socket, no need to poll router-alert sock */
	if (transport->can_send_data && events & POLLOUT)
	{
		g_assert ( (1 + moo) <= *n_fds );
		fds[moo].fd = transport->send_sock;
		fds[moo].events = POLLOUT;
		moo++;
	}

	return *n_fds = moo;
}
#endif /* CONFIG_HAVE_POLL */

/* add epoll parameters for this transports recieve socket(s), events should
 * be set to EPOLLIN to wait for incoming events (data), and EPOLLOUT to wait
 * for non-blocking write.
 *
 * returns 0 on success, -1 on failure and sets errno appropriately.
 */
#ifdef CONFIG_HAVE_EPOLL
int
pgm_transport_epoll_ctl (
	pgm_transport_t*	transport,
	int			epfd,
	int			op,		/* EPOLL_CTL_ADD, ... */
	int			events		/* EPOLLIN, EPOLLOUT */
	)
{
	if (op != EPOLL_CTL_ADD)	/* only addition currently supported */
	{
		errno = EINVAL;
		return -1;
	}
	else if (!transport->is_open)
	{
		errno = EBADF;
		return -1;
	}

	struct epoll_event event;
	int retval = 0;

	if (events & EPOLLIN)
	{
		event.events = events & (EPOLLIN | EPOLLET);
		event.data.ptr = transport;
		retval = epoll_ctl (epfd, op, transport->recv_sock, &event);
		if (retval) {
			goto out;
		}

		if (transport->can_recv)
		{
			retval = epoll_ctl (epfd, op, pgm_notify_get_fd (&transport->waiting_notify), &event);
			if (retval) {
				goto out;
			}
		}

		if (events & EPOLLET) {
			transport->is_edge_triggered_recv = TRUE;
		}
	}

	if (transport->can_send_data && events & EPOLLOUT)
	{
		event.events = events & (EPOLLOUT | EPOLLET);
		event.data.ptr = transport;
		retval = epoll_ctl (epfd, op, transport->send_sock, &event);
	}
out:
	return retval;
}
#endif

/* prod to wakeup timer thread
 *
 * returns TRUE to keep monitoring the event source.
 */

static gboolean
on_timer_notify (
	G_GNUC_UNUSED GIOChannel*	source,
	G_GNUC_UNUSED GIOCondition	condition,
	gpointer			data
	)
{
	pgm_transport_t* transport = data;

/* empty notify channel */
	pgm_notify_clear (&transport->timer_notify);

	return TRUE;
}

static gboolean
on_timer_shutdown (
	G_GNUC_UNUSED GIOChannel*	source,
	G_GNUC_UNUSED GIOCondition	condition,
	gpointer			data
	)
{
	pgm_transport_t* transport = data;
	g_main_loop_quit (transport->timer_loop);
	return FALSE;
}
	
/* Enable FEC for this transport, specifically Reed Solmon encoding RS(n,k), common
 * setting is RS(255, 223).
 *
 * inputs:
 *
 * n = FEC Block size = [k+1, 255]
 * k = original data packets == transmission group size = [2, 4, 8, 16, 32, 64, 128]
 * m = symbol size = 8 bits
 *
 * outputs:
 *
 * h = 2t = n - k = parity packets
 *
 * when h > k parity packets can be lost.
 *
 * on success, returns 0.  on invalid setting, returns -EINVAL.
 */

int
pgm_transport_set_fec (
	pgm_transport_t*	transport,
	guint			proactive_h,		/* 0 == no pro-active parity */
	gboolean		use_ondemand_parity,
	gboolean		use_varpkt_len,
	guint			default_n,
	guint			default_k
	)
{
	g_return_val_if_fail (transport != NULL, -EINVAL);
	g_return_val_if_fail ((default_k & (default_k -1)) == 0, -EINVAL);
	g_return_val_if_fail (default_k >= 2 && default_k <= 128, -EINVAL);
	g_return_val_if_fail (default_n >= default_k + 1 && default_n <= 255, -EINVAL);

	guint default_h = default_n - default_k;

	g_return_val_if_fail (proactive_h <= default_h, -EINVAL);

/* check validity of parameters */
	if ( default_k > 223 &&
		( (default_h * 223.0) / default_k ) < 1.0 )
	{
		g_error ("k/h ratio too low to generate parity data.");
		return -EINVAL;
	}

	g_static_mutex_lock (&transport->mutex);
	transport->use_proactive_parity	= proactive_h > 0;
	transport->use_ondemand_parity	= use_ondemand_parity;
	transport->use_varpkt_len	= use_varpkt_len;
	transport->rs_n			= default_n;
	transport->rs_k			= default_k;
	transport->rs_proactive_h	= proactive_h;
	transport->tg_sqn_shift		= pgm_power2_log2 (transport->rs_k);

	g_static_mutex_unlock (&transport->mutex);

	return 0;
}

/* declare transport only for sending, discard any incoming SPM, ODATA,
 * RDATA, etc, packets.
 */
int
pgm_transport_set_send_only (
	pgm_transport_t*	transport,
	gboolean		send_only
	)
{
	g_return_val_if_fail (transport != NULL, -EINVAL);

	g_static_mutex_lock (&transport->mutex);
	transport->can_recv	= !send_only;
	g_static_mutex_unlock (&transport->mutex);

	return 0;
}

/* declare transport only for receiving, no transmit window will be created
 * and no SPM broadcasts sent.
 */
int
pgm_transport_set_recv_only (
	pgm_transport_t*	transport,
	gboolean		is_passive	/* don't send any request or responses */
	)
{
	g_return_val_if_fail (transport != NULL, -EINVAL);

	g_static_mutex_lock (&transport->mutex);
	transport->can_send_data	= FALSE;
	transport->can_send_nak		= !is_passive;
	g_static_mutex_unlock (&transport->mutex);

	return 0;
}

/* on unrecoverable data loss shutdown transport from further transmission or
 * receiving.
 */
int
pgm_transport_set_close_on_failure (
	pgm_transport_t*	transport,
	gboolean		close_on_failure
	)
{
	g_return_val_if_fail (transport != NULL, -EINVAL);

	g_static_mutex_lock (&transport->mutex);
	transport->will_close_on_failure = close_on_failure;
	g_static_mutex_unlock (&transport->mutex);

	return 0;
}


#define SOCKADDR_TO_LEVEL(sa)	( (AF_INET == pgm_sockaddr_family((sa))) ? IPPROTO_IP : IPPROTO_IPV6 )
#define TRANSPORT_TO_LEVEL(t)	SOCKADDR_TO_LEVEL( &(t)->recv_gsr[0].gsr_group )


/* for any-source applications (ASM), join a new group
 */
int
pgm_transport_join_group (
	pgm_transport_t*	transport,
	struct group_req*	gr,
	gsize			len
	)
{
	g_return_val_if_fail (transport != NULL, -EINVAL);
	g_return_val_if_fail (gr != NULL, -EINVAL);
	g_return_val_if_fail (sizeof(struct group_req) == len, -EINVAL);
	g_return_val_if_fail (transport->recv_gsr_len < IP_MAX_MEMBERSHIPS, -EINVAL);

/* verify not duplicate group/interface pairing */
	for (unsigned i = 0; i < transport->recv_gsr_len; i++)
	{
		if (pgm_sockaddr_cmp ((struct sockaddr*)&gr->gr_group, (struct sockaddr*)&transport->recv_gsr[i].gsr_group)  == 0 &&
		    pgm_sockaddr_cmp ((struct sockaddr*)&gr->gr_group, (struct sockaddr*)&transport->recv_gsr[i].gsr_source) == 0 &&
			(gr->gr_interface == transport->recv_gsr[i].gsr_interface ||
			                0 == transport->recv_gsr[i].gsr_interface    )
                   )
		{
#ifdef TRANSPORT_DEBUG
			char s[INET6_ADDRSTRLEN];
			pgm_sockaddr_ntop (&gr->gr_group, s, sizeof(s));
			if (transport->recv_gsr[i].gsr_interface) {
				g_trace("INFO", "transport has already joined group %s on interface %i.", s, gr->gr_interface);
			} else {
				g_trace("INFO", "transport has already joined group %s on all interfaces.", s);
			}
#endif
			return -EINVAL;
		}
	}

	transport->recv_gsr[transport->recv_gsr_len].gsr_interface = 0;
	memcpy (&transport->recv_gsr[transport->recv_gsr_len].gsr_group, &gr->gr_group, pgm_sockaddr_len(&gr->gr_group));
	memcpy (&transport->recv_gsr[transport->recv_gsr_len].gsr_source, &gr->gr_group, pgm_sockaddr_len(&gr->gr_group));
	transport->recv_gsr_len++;
	return setsockopt(transport->recv_sock, TRANSPORT_TO_LEVEL(transport), MCAST_JOIN_GROUP, gr, len);
}

/* for any-source applications (ASM), leave a joined group.
 */
int
pgm_transport_leave_group (
	pgm_transport_t*	transport,
	struct group_req*	gr,
	gsize			len
	)
{
	g_return_val_if_fail (transport != NULL, -EINVAL);
	g_return_val_if_fail (gr != NULL, -EINVAL);
	g_return_val_if_fail (sizeof(struct group_req) == len, -EINVAL);
	g_return_val_if_fail (transport->recv_gsr_len == 0, -EINVAL);

	for (unsigned i = 0; i < transport->recv_gsr_len;)
	{
		if ((pgm_sockaddr_cmp ((struct sockaddr*)&gr->gr_group, (struct sockaddr*)&transport->recv_gsr[i].gsr_group) == 0) &&
/* drop all matching receiver entries */
		            (gr->gr_interface == 0 ||
/* drop all sources with matching interface */
			     gr->gr_interface == transport->recv_gsr[i].gsr_interface) )
		{
			transport->recv_gsr_len--;
			if (i < (IP_MAX_MEMBERSHIPS-1))
			{
				memmove (&transport->recv_gsr[i], &transport->recv_gsr[i+1], (transport->recv_gsr_len - i) * sizeof(struct group_source_req));
				continue;
			}
		}
		i++;
	}
	return setsockopt(transport->recv_sock, TRANSPORT_TO_LEVEL(transport), MCAST_LEAVE_GROUP, gr, len);
}

/* for any-source applications (ASM), turn off a given source
 */
int
pgm_transport_block_source (
	pgm_transport_t*	transport,
	struct group_source_req* gsr,
	gsize			len
	)
{
	g_return_val_if_fail (transport != NULL, -EINVAL);
	g_return_val_if_fail (gsr != NULL, -EINVAL);
	g_return_val_if_fail (sizeof(struct group_source_req) == len, -EINVAL);
	return setsockopt(transport->recv_sock, TRANSPORT_TO_LEVEL(transport), MCAST_BLOCK_SOURCE, gsr, len);
}

/* for any-source applications (ASM), re-allow a blocked source
 */
int
pgm_transport_unblock_source (
	pgm_transport_t*	transport,
	struct group_source_req* gsr,
	gsize			len
	)
{
	g_return_val_if_fail (transport != NULL, -EINVAL);
	g_return_val_if_fail (gsr != NULL, -EINVAL);
	g_return_val_if_fail (sizeof(struct group_source_req) == len, -EINVAL);
	return setsockopt(transport->recv_sock, TRANSPORT_TO_LEVEL(transport), MCAST_UNBLOCK_SOURCE, gsr, len);
}

/* for controlled-source applications (SSM), join each group/source pair.
 *
 * SSM joins are allowed on top of ASM in order to merge a remote source onto the local segment.
 */
int
pgm_transport_join_source_group (
	pgm_transport_t*	transport,
	struct group_source_req* gsr,
	gsize			len
	)
{
	g_return_val_if_fail (transport != NULL, -EINVAL);
	g_return_val_if_fail (gsr != NULL, -EINVAL);
	g_return_val_if_fail (sizeof(struct group_source_req) == len, -EINVAL);
	g_return_val_if_fail (transport->recv_gsr_len < IP_MAX_MEMBERSHIPS, -EINVAL);

/* verify if existing group/interface pairing */
	for (unsigned i = 0; i < transport->recv_gsr_len; i++)
	{
		if (pgm_sockaddr_cmp ((struct sockaddr*)&gsr->gsr_group, (struct sockaddr*)&transport->recv_gsr[i].gsr_group) == 0 &&
			(gsr->gsr_interface == transport->recv_gsr[i].gsr_interface ||
			                  0 == transport->recv_gsr[i].gsr_interface    )
                   )
		{
			if (pgm_sockaddr_cmp ((struct sockaddr*)&gsr->gsr_source, (struct sockaddr*)&transport->recv_gsr[i].gsr_source) == 0)
			{
#ifdef TRANSPORT_DEBUG
				char s1[INET6_ADDRSTRLEN], s2[INET6_ADDRSTRLEN];
				pgm_sockaddr_ntop (&gsr->gsr_group, s1, sizeof(s1));
				pgm_sockaddr_ntop (&gsr->gsr_source, s2, sizeof(s2));
				if (transport->recv_gsr[i].gsr_interface) {
					g_trace("INFO", "transport has already joined group %s from source %s on interface %i.", s1, s2, gsr->gsr_interface);
				} else {
					g_trace("INFO", "transport has already joined group %s from source %s on all interfaces.", s1, s2);
				}
#endif
				return -EINVAL;
			}
			break;
		}
	}

	memcpy (&transport->recv_gsr[transport->recv_gsr_len], &gsr, sizeof(struct group_source_req));
	transport->recv_gsr_len++;
	return setsockopt(transport->recv_sock, TRANSPORT_TO_LEVEL(transport), MCAST_JOIN_SOURCE_GROUP, gsr, len);
}

/* for controlled-source applications (SSM), leave each group/source pair
 */
int
pgm_transport_leave_source_group (
	pgm_transport_t*	transport,
	struct group_source_req* gsr,
	gsize			len
	)
{
	g_return_val_if_fail (transport != NULL, -EINVAL);
	g_return_val_if_fail (gsr != NULL, -EINVAL);
	g_return_val_if_fail (sizeof(struct group_source_req) == len, -EINVAL);
	g_return_val_if_fail (transport->recv_gsr_len == 0, -EINVAL);

/* verify if existing group/interface pairing */
	for (unsigned i = 0; i < transport->recv_gsr_len; i++)
	{
		if (pgm_sockaddr_cmp ((struct sockaddr*)&gsr->gsr_group, (struct sockaddr*)&transport->recv_gsr[i].gsr_group)   == 0 &&
		    pgm_sockaddr_cmp ((struct sockaddr*)&gsr->gsr_source, (struct sockaddr*)&transport->recv_gsr[i].gsr_source) == 0 &&
		    gsr->gsr_interface == transport->recv_gsr[i].gsr_interface)
		{
			transport->recv_gsr_len--;
			if (i < (IP_MAX_MEMBERSHIPS-1))
			{
				memmove (&transport->recv_gsr[i], &transport->recv_gsr[i+1], (transport->recv_gsr_len - i) * sizeof(struct group_source_req));
				break;
			}
		}
	}

	return setsockopt(transport->recv_sock, TRANSPORT_TO_LEVEL(transport), MCAST_LEAVE_SOURCE_GROUP, gsr, len);
}

int
pgm_transport_msfilter (
	pgm_transport_t*	transport,
	struct group_filter*	gf_list,
	gsize			len
	)
{
	g_return_val_if_fail (transport != NULL, -EINVAL);
	g_return_val_if_fail (gf_list != NULL, -EINVAL);
	g_return_val_if_fail (len > 0, -EINVAL);
	g_return_val_if_fail (GROUP_FILTER_SIZE(gf_list->gf_numsrc) == len, -EINVAL);
	return setsockopt(transport->recv_sock, TRANSPORT_TO_LEVEL(transport), MCAST_MSFILTER, gf_list, len);
}

/* TODO: this should be in on_io_data to be more streamlined, or a generic options parser.
 *
 * returns 1 if opt_fragment is found, otherwise 0 is returned.
 */

int
_pgm_get_opt_fragment (
	struct pgm_opt_header*		opt_header,
	struct pgm_opt_fragment**	opt_fragment
	)
{
	g_assert (opt_header->opt_type   == PGM_OPT_LENGTH);
	g_assert (opt_header->opt_length == sizeof(struct pgm_opt_length));

/* always at least two options, first is always opt_length */
	do {
		opt_header = (struct pgm_opt_header*)((char*)opt_header + opt_header->opt_length);

		if ((opt_header->opt_type & PGM_OPT_MASK) == PGM_OPT_FRAGMENT)
		{
			*opt_fragment = (struct pgm_opt_fragment*)(opt_header + 1);
			return 1;
		}

	} while (!(opt_header->opt_type & PGM_OPT_END));

	*opt_fragment = NULL;
	return 0;
}

/* eof */