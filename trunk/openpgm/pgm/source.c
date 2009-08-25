/* vim:ts=8:sts=8:sw=4:noai:noexpandtab
 *
 * PGM source transport.
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
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#ifdef CONFIG_HAVE_POLL
#	include <poll.h>
#endif
#ifdef CONFIG_HAVE_EPOLL
#	include <sys/epoll.h>
#endif

#include <glib.h>

#ifdef G_OS_UNIX
#	include <netdb.h>
#	include <net/if.h>
#	include <netinet/in.h>
#	include <netinet/ip.h>
#	include <netinet/udp.h>
#	include <sys/socket.h>
#	include <sys/time.h>
#	include <arpa/inet.h>
#else
#	include <ws2tcpip.h>
#endif

#include "pgm/transport.h"
#include "pgm/source.h"
#include "pgm/if.h"
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

#define SOURCE_DEBUG
//#define SPM_DEBUG

#ifndef SOURCE_DEBUG
#	define g_trace(m,...)		while (0)
#else
#	include <ctype.h>
#	ifdef SPM_DEBUG
#		define g_trace(m,...)		g_debug(__VA_ARGS__)
#	else
#		define g_trace(m,...)		do { if (strcmp((m),"SPM")) { g_debug(__VA_ARGS__); } } while (0)
#	endif
#endif


/* locals */
static gboolean send_spm (pgm_transport_t* const);
static void reset_heartbeat_spm (pgm_transport_t* const);
static gboolean send_ncf (pgm_transport_t* const, const struct sockaddr* const, const struct sockaddr* const, const guint32, const gboolean);
static gboolean send_ncf_list (pgm_transport_t* const, const struct sockaddr* const, const struct sockaddr*, pgm_sqn_list_t* const, const gboolean);
static GIOStatus send_odata (pgm_transport_t* const, struct pgm_sk_buff_t* const, gsize*);
static GIOStatus send_odata_copy (pgm_transport_t* const, gconstpointer, const gsize, gsize*);
static GIOStatus send_odatav (pgm_transport_t* const, const struct pgm_iovec* const, const guint, gsize*);
static gboolean send_rdata (pgm_transport_t* const, struct pgm_sk_buff_t* const);


static inline
gboolean
peer_is_source (
	const pgm_peer_t*	peer
	)
{
	return (NULL == peer);
}

static inline
gboolean
peer_is_peer (
	const pgm_peer_t*	peer
	)
{
	return (NULL != peer);
}

static inline
void
reset_spmr_timer (
	pgm_peer_t* const	peer
	)
{
	g_static_mutex_lock (&peer->mutex);
	peer->spmr_expiry = 0;
	g_static_mutex_unlock (&peer->mutex);
}

/* Linux 2.6 limited to millisecond resolution with conventional timers, however RDTSC
 * and future high-resolution timers allow nanosecond resolution.  Current ethernet technology
 * is limited to microseconds at best so we'll sit there for a bit.
 *
 * on success, returns TRUE.  on invalid setting, returns FALSE.
 */

gboolean
pgm_transport_set_ambient_spm (
	pgm_transport_t* const	transport,
	const guint		spm_ambient_interval	/* in microseconds */
	)
{
	g_return_val_if_fail (NULL != transport, FALSE);
	g_return_val_if_fail (!transport->is_bound, FALSE);
	g_return_val_if_fail (spm_ambient_interval > 0, FALSE);

	g_static_mutex_lock (&transport->mutex);
	transport->spm_ambient_interval = spm_ambient_interval;
	g_static_mutex_unlock (&transport->mutex);
	return TRUE;
}

/* an array of intervals appropriately tuned till ambient period is reached.
 *
 * array is zero leaded for ambient state, and zero terminated for easy detection.
 *
 * on success, returns TRUE.  on invalid setting, returns FALSE.
 */

gboolean
pgm_transport_set_heartbeat_spm (
	pgm_transport_t* const	transport,
	const guint* const	spm_heartbeat_interval,
	const int		len
	)
{
	g_return_val_if_fail (NULL != transport, FALSE);
	g_return_val_if_fail (!transport->is_bound, FALSE);
	g_return_val_if_fail (len > 0, FALSE);
	for (int i = 0; i < len; i++) {
		g_return_val_if_fail (spm_heartbeat_interval[i] > 0, FALSE);
	}

	g_static_mutex_lock (&transport->mutex);
	if (transport->spm_heartbeat_interval)
		g_free (transport->spm_heartbeat_interval);
	transport->spm_heartbeat_interval = g_malloc (sizeof(guint) * (len+2));
	memcpy (&transport->spm_heartbeat_interval[1], spm_heartbeat_interval, sizeof(guint) * len);
	transport->spm_heartbeat_interval[0] = transport->spm_heartbeat_interval[len] = 0;
	g_static_mutex_unlock (&transport->mutex);
	return TRUE;
}

/* 0 < txw_sqns < one less than half sequence space
 *
 * on success, returns TRUE.  on invalid setting, returns FALSE.
 */

gboolean
pgm_transport_set_txw_sqns (
	pgm_transport_t* const	transport,
	const guint		sqns
	)
{
	g_return_val_if_fail (NULL != transport, FALSE);
	g_return_val_if_fail (!transport->is_bound, FALSE);
	g_return_val_if_fail (sqns < ((UINT32_MAX/2)-1), FALSE);
	g_return_val_if_fail (sqns > 0, FALSE);

	g_static_mutex_lock (&transport->mutex);
	transport->txw_sqns = sqns;
	g_static_mutex_unlock (&transport->mutex);
	return TRUE;
}

/* 0 < secs < ( txw_sqns / txw_max_rte )
 *
 * can only be enforced upon bind.
 *
 * on success, returns TRUE.  on invalid setting, returns FALSE.
 */

gboolean
pgm_transport_set_txw_secs (
	pgm_transport_t* const	transport,
	const guint		secs
	)
{
	g_return_val_if_fail (NULL != transport, FALSE);
	g_return_val_if_fail (!transport->is_bound, FALSE);
	g_return_val_if_fail (secs > 0, FALSE);

	g_static_mutex_lock (&transport->mutex);
	transport->txw_secs = secs;
	g_static_mutex_unlock (&transport->mutex);
	return TRUE;
}

/* 0 < txw_max_rte < interface capacity
 *
 *  10mb :   1250000
 * 100mb :  12500000
 *   1gb : 125000000
 *
 * no practical way to determine upper limit and enforce.
 *
 * on success, returns TRUE.  on invalid setting, returns FALSE.
 */

gboolean
pgm_transport_set_txw_max_rte (
	pgm_transport_t* const	transport,
	const guint		max_rte
	)
{
	g_return_val_if_fail (transport != NULL, FALSE);
	g_return_val_if_fail (!transport->is_bound, FALSE);
	g_return_val_if_fail (max_rte > 0, FALSE);

	g_static_mutex_lock (&transport->mutex);
	transport->txw_max_rte = max_rte;
	g_static_mutex_unlock (&transport->mutex);
	return TRUE;
}

/* prototype of function to send pro-active parity NAKs.
 */
static
gboolean
pgm_schedule_proactive_nak (
	pgm_transport_t*	transport,
	guint32			nak_tg_sqn	/* transmission group (shifted) */
	)
{
	g_return_val_if_fail (NULL != transport, FALSE);
	gboolean status = pgm_txw_retransmit_push (transport->window,
						   nak_tg_sqn | transport->rs_proactive_h,
						   TRUE /* is_parity */,
						   transport->tg_sqn_shift);
	return status;
}

/* a deferred request for RDATA, now processing in the timer thread, we check the transmit
 * window to see if the packet exists and forward on, maintaining a lock until the queue is
 * empty.
 *
 * returns TRUE on success, returns FALSE if operation would block.
 */

gboolean
pgm_on_deferred_nak (
	pgm_transport_t* const	transport
	)
{
/* pre-conditions */
	g_assert (NULL != transport);

/* We can flush queue and block all odata, or process one set, or process each
 * sequence number individually.
 */

/* parity packets are re-numbered across the transmission group with index h, sharing the space
 * with the original packets.  beyond the transmission group size (k), the PGM option OPT_PARITY_GRP
 * provides the extra offset value.
 */

/* peek from the retransmit queue so we can eliminate duplicate NAKs up until the repair packet
 * has been retransmitted.
 */
	g_static_rw_lock_reader_lock (&transport->window_lock);
	struct pgm_sk_buff_t* skb = pgm_txw_retransmit_try_peek (transport->window);
	if (skb) {
		if (!send_rdata (transport, skb)) {
			g_static_rw_lock_reader_unlock (&transport->window_lock);
			return FALSE;
		}

/* now remove sequence number from retransmit queue, re-enabling NAK processing for this sequence number */
		pgm_txw_retransmit_remove_head (transport->window);
	}
	g_static_rw_lock_reader_unlock (&transport->window_lock);
	return TRUE;
}

/* SPMR indicates if multicast to cancel own SPMR, or unicast to send SPM.
 *
 * rate limited to 1/IHB_MIN per TSI (13.4).
 *
 * if SPMR was valid, returns TRUE, if invalid returns FALSE.
 */

gboolean
pgm_on_spmr (
	pgm_transport_t* const		transport,
	pgm_peer_t* const		peer,		/* maybe NULL if transport is source */
	struct pgm_sk_buff_t* const	skb
	)
{
/* pre-conditions */
	g_assert (NULL != transport);
	g_assert (NULL != skb);

	g_trace ("INFO","pgm_on_spmr (transport:%p peer:%p skb:%p)",
		(gpointer)transport, (gpointer)peer, (gpointer)skb);

	if (!pgm_verify_spmr (skb)) {
		g_trace ("DEBUG","Malformed SPMR rejected.");
		return FALSE;
	}

	if (peer_is_source (peer))
		send_spm (transport);
	else {
		g_trace ("INFO", "suppressing SPMR due to peer multicast SPMR.");
		reset_spmr_timer (peer);
	}
	return TRUE;
}

/* NAK requesting RDATA transmission for a sending transport, only valid if
 * sequence number(s) still in transmission window.
 *
 * we can potentially have different IP versions for the NAK packet to the send group.
 *
 * TODO: fix IPv6 AFIs
 *
 * take in a NAK and pass off to an asynchronous queue for another thread to process
 *
 * if NAK is valid, returns TRUE.  on error, FALSE is returned.
 */

gboolean
pgm_on_nak (
	pgm_transport_t* const		transport,
	struct pgm_sk_buff_t* const	skb
	)
{
/* pre-conditions */
	g_assert (NULL != transport);
	g_assert (NULL != skb);

	g_trace ("INFO","pgm_on_nak (transport:%p skb:%p)",
		(gpointer)transport, (gpointer)skb);

	const gboolean is_parity = skb->pgm_header->pgm_options & PGM_OPT_PARITY;
	if (is_parity) {
		transport->cumulative_stats[PGM_PC_SOURCE_PARITY_NAKS_RECEIVED]++;
		if (!transport->use_ondemand_parity) {
			g_trace ("DEBUG","Parity NAK rejected as on-demand parity is not enabled.");
			transport->cumulative_stats[PGM_PC_SOURCE_MALFORMED_NAKS]++;
			return FALSE;
		}
	} else
		transport->cumulative_stats[PGM_PC_SOURCE_SELECTIVE_NAKS_RECEIVED]++;

	if (!pgm_verify_nak (skb)) {
		g_trace ("DEBUG","Malformed NAK rejected.");
		transport->cumulative_stats[PGM_PC_SOURCE_MALFORMED_NAKS]++;
		return FALSE;
	}

	const struct pgm_nak*  nak  = (struct pgm_nak*) skb->data;
	const struct pgm_nak6* nak6 = (struct pgm_nak6*)skb->data;
		
/* NAK_SRC_NLA contains our transport unicast NLA */
	struct sockaddr_storage nak_src_nla;
	pgm_nla_to_sockaddr (&nak->nak_src_nla_afi, (struct sockaddr*)&nak_src_nla);
	if (pgm_sockaddr_cmp ((struct sockaddr*)&nak_src_nla, (struct sockaddr*)&transport->send_addr) != 0)
	{
		char saddr[INET6_ADDRSTRLEN];
		pgm_sockaddr_ntop ((struct sockaddr*)&nak_src_nla, saddr, sizeof(saddr));
		g_trace ("DEBUG","NAK rejected for unmatched NLA: %s", saddr);
		transport->cumulative_stats[PGM_PC_SOURCE_MALFORMED_NAKS]++;
		return FALSE;
	}

/* NAK_GRP_NLA containers our transport multicast group */ 
	struct sockaddr_storage nak_grp_nla;
	pgm_nla_to_sockaddr ((nak->nak_src_nla_afi == AFI_IP6) ? &nak6->nak6_grp_nla_afi : &nak->nak_grp_nla_afi, (struct sockaddr*)&nak_grp_nla);
	if (pgm_sockaddr_cmp ((struct sockaddr*)&nak_grp_nla, (struct sockaddr*)&transport->send_gsr.gsr_group) != 0)
	{
		char sgroup[INET6_ADDRSTRLEN];
		pgm_sockaddr_ntop ((struct sockaddr*)&nak_src_nla, sgroup, sizeof(sgroup));
		g_trace ("DEBUG","NAK rejected as targeted for different multicast group: %s", sgroup);
		transport->cumulative_stats[PGM_PC_SOURCE_MALFORMED_NAKS]++;
		return FALSE;
	}

/* create queue object */
	pgm_sqn_list_t sqn_list;
	sqn_list.sqn[0] = g_ntohl (nak->nak_sqn);
	sqn_list.len = 1;

	g_trace ("INFO", "nak_sqn %" G_GUINT32_FORMAT, sqn_list.sqn[0]);

/* check NAK list */
	const guint32* nak_list = NULL;
	guint nak_list_len = 0;
	if (skb->pgm_header->pgm_options & PGM_OPT_PRESENT)
	{
		const struct pgm_opt_length* opt_len = (nak->nak_src_nla_afi == AFI_IP6) ?
							(const struct pgm_opt_length*)(nak6 + 1) :
							(const struct pgm_opt_length*)(nak  + 1);
		if (opt_len->opt_type != PGM_OPT_LENGTH) {
			g_trace ("DEBUG","Malformed NAK rejected.");
			transport->cumulative_stats[PGM_PC_SOURCE_MALFORMED_NAKS]++;
			return FALSE;
		}
		if (opt_len->opt_length != sizeof(struct pgm_opt_length)) {
			g_trace ("DEBUG","Malformed NAK rejected.");
			transport->cumulative_stats[PGM_PC_SOURCE_MALFORMED_NAKS]++;
			return FALSE;
		}
/* TODO: check for > 16 options & past packet end */
		const struct pgm_opt_header* opt_header = (const struct pgm_opt_header*)opt_len;
		do {
			opt_header = (const struct pgm_opt_header*)((const char*)opt_header + opt_header->opt_length);
			if ((opt_header->opt_type & PGM_OPT_MASK) == PGM_OPT_NAK_LIST) {
				nak_list = ((const struct pgm_opt_nak_list*)(opt_header + 1))->opt_sqn;
				nak_list_len = ( opt_header->opt_length - sizeof(struct pgm_opt_header) - sizeof(guint8) ) / sizeof(guint32);
				break;
			}
		} while (!(opt_header->opt_type & PGM_OPT_END));
	}

/* nak list numbers */
	if (nak_list_len > 63) {
		g_trace ("DEBUG","Malformed NAK rejected on too long sequence list.");
		return FALSE;
	}
		
	for (unsigned i = 0; i < nak_list_len; i++)
	{
		sqn_list.sqn[sqn_list.len++] = g_ntohl (*nak_list);
		nak_list++;
	}

/* send NAK confirm packet immediately, then defer to timer thread for a.s.a.p
 * delivery of the actual RDATA packets.
 */
	if (nak_list_len)
		send_ncf_list (transport, (struct sockaddr*)&nak_src_nla, (struct sockaddr*)&nak_grp_nla, &sqn_list, is_parity);
	else
		send_ncf (transport, (struct sockaddr*)&nak_src_nla, (struct sockaddr*)&nak_grp_nla, sqn_list.sqn[0], is_parity);

/* queue retransmit requests */
	for (unsigned i = 0; i < sqn_list.len; i++)
		pgm_txw_retransmit_push (transport->window, sqn_list.sqn[i], is_parity, transport->tg_sqn_shift);
	return TRUE;
}

/* Null-NAK, or N-NAK propogated by a DLR for hand waving excitement
 *
 * if NNAK is valid, returns TRUE.  on error, FALSE is returned.
 */

gboolean
pgm_on_nnak (
	pgm_transport_t* const		transport,
	struct pgm_sk_buff_t* const	skb
	)
{
/* pre-conditions */
	g_assert (NULL != transport);
	g_assert (NULL != skb);

	g_trace ("INFO","pgm_on_nnak (transport:%p skb:%p)",
		(gpointer)transport, (gpointer)skb);

	transport->cumulative_stats[PGM_PC_SOURCE_SELECTIVE_NNAK_PACKETS_RECEIVED]++;

	if (!pgm_verify_nnak (skb)) {
		transport->cumulative_stats[PGM_PC_SOURCE_NNAK_ERRORS]++;
		return FALSE;
	}

	const struct pgm_nak*  nnak  = (struct pgm_nak*) skb->data;
	const struct pgm_nak6* nnak6 = (struct pgm_nak6*)skb->data;
		
/* NAK_SRC_NLA contains our transport unicast NLA */
	struct sockaddr_storage nnak_src_nla;
	pgm_nla_to_sockaddr (&nnak->nak_src_nla_afi, (struct sockaddr*)&nnak_src_nla);

	if (pgm_sockaddr_cmp ((struct sockaddr*)&nnak_src_nla, (struct sockaddr*)&transport->send_addr) != 0)
	{
		transport->cumulative_stats[PGM_PC_SOURCE_NNAK_ERRORS]++;
		return FALSE;
	}

/* NAK_GRP_NLA containers our transport multicast group */ 
	struct sockaddr_storage nnak_grp_nla;
	pgm_nla_to_sockaddr ((nnak->nak_src_nla_afi == AFI_IP6) ? &nnak6->nak6_grp_nla_afi : &nnak->nak_grp_nla_afi, (struct sockaddr*)&nnak_grp_nla);
	if (pgm_sockaddr_cmp ((struct sockaddr*)&nnak_grp_nla, (struct sockaddr*)&transport->send_gsr.gsr_group) != 0)
	{
		transport->cumulative_stats[PGM_PC_SOURCE_NNAK_ERRORS]++;
		return FALSE;
	}

/* check NNAK list */
	guint nnak_list_len = 0;
	if (skb->pgm_header->pgm_options & PGM_OPT_PRESENT)
	{
		const struct pgm_opt_length* opt_len = (nnak->nak_src_nla_afi == AFI_IP6) ?
							(const struct pgm_opt_length*)(nnak6 + 1) :
							(const struct pgm_opt_length*)(nnak + 1);
		if (opt_len->opt_type != PGM_OPT_LENGTH) {
			transport->cumulative_stats[PGM_PC_SOURCE_NNAK_ERRORS]++;
			return FALSE;
		}
		if (opt_len->opt_length != sizeof(struct pgm_opt_length)) {
			transport->cumulative_stats[PGM_PC_SOURCE_NNAK_ERRORS]++;
			return FALSE;
		}
/* TODO: check for > 16 options & past packet end */
		const struct pgm_opt_header* opt_header = (const struct pgm_opt_header*)opt_len;
		do {
			opt_header = (const struct pgm_opt_header*)((const char*)opt_header + opt_header->opt_length);
			if ((opt_header->opt_type & PGM_OPT_MASK) == PGM_OPT_NAK_LIST) {
				nnak_list_len = ( opt_header->opt_length - sizeof(struct pgm_opt_header) - sizeof(guint8) ) / sizeof(guint32);
				break;
			}
		} while (!(opt_header->opt_type & PGM_OPT_END));
	}

	transport->cumulative_stats[PGM_PC_SOURCE_SELECTIVE_NNAKS_RECEIVED] += 1 + nnak_list_len;
	return TRUE;
}

/* ambient/heartbeat SPM's
 *
 * heartbeat: ihb_tmr decaying between ihb_min and ihb_max 2x after last packet
 *
 * on success, 0 is returned.  on error, -1 is returned, and errno set
 * appropriately.
 */

static
gboolean
send_spm (
	pgm_transport_t*	transport
	)
{
/* pre-conditions */
	g_assert (NULL != transport);

	g_trace ("SPM","send_spm (transport:%p", (gpointer)transport);

	g_static_mutex_lock (&transport->mutex);
	const gboolean status = pgm_send_spm_unlocked (transport);
	g_static_mutex_unlock (&transport->mutex);
	return status;
}

gboolean
pgm_send_spm_unlocked (
	pgm_transport_t*	transport
	)
{
/* pre-conditions */
	g_assert (NULL != transport);
	g_assert (NULL != transport->window);

	g_trace ("SPM","pgm_send_spm_unlocked (transport:%p", (gpointer)transport);

	gsize tpdu_length = sizeof(struct pgm_header);
	if (AF_INET == pgm_sockaddr_family(&transport->send_gsr.gsr_group))
		tpdu_length += sizeof(struct pgm_spm);
	else
		tpdu_length += sizeof(struct pgm_spm6);
	if (transport->use_proactive_parity || transport->use_ondemand_parity)
		tpdu_length += sizeof(struct pgm_opt_length) +
			       sizeof(struct pgm_opt_header) +
			       sizeof(struct pgm_opt_parity_prm);
	guint8 buf[ tpdu_length ];
	struct pgm_header *header = (struct pgm_header*)buf;
	struct pgm_spm *spm = (struct pgm_spm*)(header + 1);
	struct pgm_spm6 *spm6 = (struct pgm_spm6*)(header + 1);
	memcpy (header->pgm_gsi, &transport->tsi.gsi, sizeof(pgm_gsi_t));
	header->pgm_sport       = transport->tsi.sport;
	header->pgm_dport       = transport->dport;
	header->pgm_type        = PGM_SPM;
	header->pgm_options     = 0;
	header->pgm_tsdu_length = 0;

/* SPM */
	spm->spm_sqn		= g_htonl (transport->spm_sqn);
	g_static_rw_lock_reader_lock (&transport->window_lock);
	spm->spm_trail		= g_htonl (pgm_txw_trail(transport->window));
	spm->spm_lead		= g_htonl (pgm_txw_lead(transport->window));
	g_static_rw_lock_reader_unlock (&transport->window_lock);
/* our nla */
	pgm_sockaddr_to_nla ((struct sockaddr*)&transport->send_addr, (char*)&spm->spm_nla_afi);

/* OPT_PARITY_PRM */
	if (transport->use_proactive_parity || transport->use_ondemand_parity)
	{
		header->pgm_options     = PGM_OPT_PRESENT | PGM_OPT_NETWORK;
		struct pgm_opt_length* opt_len;
		if (AF_INET == pgm_sockaddr_family(&transport->send_gsr.gsr_group))
			opt_len = (struct pgm_opt_length*)(spm + 1);
		else
			opt_len = (struct pgm_opt_length*)(spm6 + 1);
		opt_len->opt_type       = PGM_OPT_LENGTH;
		opt_len->opt_length     = sizeof(struct pgm_opt_length);
		opt_len->opt_total_length = g_htons ( sizeof(struct pgm_opt_length) +
					              sizeof(struct pgm_opt_header) +
					              sizeof(struct pgm_opt_parity_prm) );
		struct pgm_opt_header* opt_header = (struct pgm_opt_header*)(opt_len + 1);
		opt_header->opt_type    = PGM_OPT_PARITY_PRM | PGM_OPT_END;
		opt_header->opt_length  = sizeof(struct pgm_opt_header) + sizeof(struct pgm_opt_parity_prm);
		struct pgm_opt_parity_prm* opt_parity_prm = (struct pgm_opt_parity_prm*)(opt_header + 1);
		opt_parity_prm->opt_reserved = (transport->use_proactive_parity ? PGM_PARITY_PRM_PRO : 0) |
					       (transport->use_ondemand_parity ? PGM_PARITY_PRM_OND : 0);
		opt_parity_prm->parity_prm_tgs = g_htonl (transport->rs_k);
	}

/* checksum optional for SPMs */
	header->pgm_checksum	= 0;
	header->pgm_checksum	= pgm_csum_fold (pgm_csum_partial ((char*)header, tpdu_length, 0));

	const gssize sent = pgm_sendto (transport,
					TRUE,				/* rate limited */
					TRUE,				/* with router alert */
					header,
					tpdu_length,
					(struct sockaddr*)&transport->send_gsr.gsr_group,
					pgm_sockaddr_len(&transport->send_gsr.gsr_group));
	if (-1 == sent && EAGAIN == errno)
		return FALSE;
/* advance SPM sequence only on successful transmission */
	transport->spm_sqn++;
	transport->cumulative_stats[PGM_PC_SOURCE_BYTES_SENT] += tpdu_length;
	return TRUE;
}

/* send a NAK confirm (NCF) message with provided sequence number list.
 *
 * on success, TRUE is returned, returns FALSE if operation would block.
 */

static
gboolean
send_ncf (
	pgm_transport_t* const		transport,
	const struct sockaddr* const	nak_src_nla,
	const struct sockaddr* const	nak_grp_nla,
	const guint32			sequence,
	const gboolean			is_parity		/* send parity NCF */
	)
{
/* pre-conditions */
	g_assert (NULL != transport);
	g_assert (NULL != nak_src_nla);
	g_assert (NULL != nak_grp_nla);
	g_assert (pgm_sockaddr_family(nak_src_nla) == pgm_sockaddr_family(nak_grp_nla));

#ifdef SOURCE_DEBUG
	char saddr[INET6_ADDRSTRLEN], gaddr[INET6_ADDRSTRLEN];
	pgm_sockaddr_ntop (nak_src_nla, saddr, sizeof(saddr));
	pgm_sockaddr_ntop (nak_grp_nla, gaddr, sizeof(gaddr));
	g_trace ("INFO", "send_ncf (transport:%p nak-src-nla:%s nak-grp-nla:%s sequence:%" G_GUINT32_FORMAT" is-parity:%s)",
		(gpointer)transport,
		saddr,
		gaddr,
		sequence,
		is_parity ? "TRUE": "FALSE"
		);
#endif

	gsize tpdu_length = sizeof(struct pgm_header);
	tpdu_length += (AF_INET == pgm_sockaddr_family(nak_src_nla)) ? sizeof(struct pgm_nak) : sizeof(struct pgm_nak6);
	guint8 buf[ tpdu_length ];
	struct pgm_header* header = (struct pgm_header*)buf;
	struct pgm_nak*  ncf  = (struct pgm_nak*) (header + 1);
	struct pgm_nak6* ncf6 = (struct pgm_nak6*)(header + 1);
	memcpy (header->pgm_gsi, &transport->tsi.gsi, sizeof(pgm_gsi_t));
	header->pgm_sport	= transport->tsi.sport;
	header->pgm_dport	= transport->dport;
	header->pgm_type        = PGM_NCF;
        header->pgm_options     = is_parity ? PGM_OPT_PARITY : 0;
        header->pgm_tsdu_length = 0;

/* NCF */
	ncf->nak_sqn		= g_htonl (sequence);

/* source nla */
	pgm_sockaddr_to_nla (nak_src_nla, (char*)&ncf->nak_src_nla_afi);

/* group nla */
	pgm_sockaddr_to_nla (nak_grp_nla, (ncf->nak_src_nla_afi == AFI_IP6) ?
						(char*)&ncf6->nak6_grp_nla_afi :
						(char*)&ncf->nak_grp_nla_afi );
        header->pgm_checksum    = 0;
        header->pgm_checksum	= pgm_csum_fold (pgm_csum_partial ((char*)header, tpdu_length, 0));

	const gssize sent = pgm_sendto (transport,
					FALSE,			/* not rate limited */
					TRUE,			/* with router alert */
					header,
					tpdu_length,
					(struct sockaddr*)&transport->send_gsr.gsr_group,
					pgm_sockaddr_len(&transport->send_gsr.gsr_group));
	if (-1 == sent && EAGAIN == errno)
		return FALSE;
	transport->cumulative_stats[PGM_PC_SOURCE_BYTES_SENT] += tpdu_length;
	return TRUE;
}

/* A NCF packet with a OPT_NAK_LIST option extension
 *
 * on success, 0 is returned.  on error, -1 is returned, and errno set
 * appropriately.
 */

static
gboolean
send_ncf_list (
	pgm_transport_t* const 		transport,
	const struct sockaddr* const	nak_src_nla,
	const struct sockaddr* const	nak_grp_nla,
	pgm_sqn_list_t* const		sqn_list,		/* will change to network-order */
	const gboolean			is_parity		/* send parity NCF */
	)
{
/* pre-conditions */
	g_assert (NULL != transport);
	g_assert (NULL != nak_src_nla);
	g_assert (NULL != nak_grp_nla);
	g_assert (sqn_list->len > 1);
	g_assert (sqn_list->len <= 63);
	g_assert (pgm_sockaddr_family(nak_src_nla) == pgm_sockaddr_family(nak_grp_nla));

#ifdef SOURCE_DEBUG
	char saddr[INET6_ADDRSTRLEN], gaddr[INET6_ADDRSTRLEN];
	char list[1024];
	pgm_sockaddr_ntop (nak_src_nla, saddr, sizeof(saddr));
	pgm_sockaddr_ntop (nak_grp_nla, gaddr, sizeof(gaddr));
	sprintf (list, "%" G_GUINT32_FORMAT, sqn_list->sqn[0]);
	for (unsigned i = 1; i < sqn_list->len; i++) {
		char sequence[2 + strlen("4294967295")];
		sprintf (sequence, " %" G_GUINT32_FORMAT, sqn_list->sqn[i]);
		strcat (list, sequence);
	}
	g_trace ("INFO", "send_ncf_list (transport:%p nak-src-nla:%s nak-grp-nla:%s sqn-list:[%s] is-parity:%s)",
		(gpointer)transport,
		saddr,
		gaddr,
		list,
		is_parity ? "TRUE": "FALSE"
		);
#endif

	gsize tpdu_length = sizeof(struct pgm_header) +
			    sizeof(struct pgm_opt_length) +		/* includes header */
			    sizeof(struct pgm_opt_header) + sizeof(struct pgm_opt_nak_list) +
			    ( (sqn_list->len-1) * sizeof(guint32) );
	tpdu_length += (AF_INET == pgm_sockaddr_family(nak_src_nla)) ? sizeof(struct pgm_nak) : sizeof(struct pgm_nak6);
	guint8 buf[ tpdu_length ];
	struct pgm_header* header = (struct pgm_header*)buf;
	struct pgm_nak*  ncf  = (struct pgm_nak*) (header + 1);
	struct pgm_nak6* ncf6 = (struct pgm_nak6*)(header + 1);
	memcpy (header->pgm_gsi, &transport->tsi.gsi, sizeof(pgm_gsi_t));
	header->pgm_sport	= transport->tsi.sport;
	header->pgm_dport	= transport->dport;
	header->pgm_type        = PGM_NCF;
        header->pgm_options     = is_parity ? (PGM_OPT_PRESENT | PGM_OPT_NETWORK | PGM_OPT_PARITY) : (PGM_OPT_PRESENT | PGM_OPT_NETWORK);
        header->pgm_tsdu_length = 0;
/* NCF */
	ncf->nak_sqn		= g_htonl (sqn_list->sqn[0]);

/* source nla */
	pgm_sockaddr_to_nla (nak_src_nla, (char*)&ncf->nak_src_nla_afi);

/* group nla */
	pgm_sockaddr_to_nla (nak_grp_nla, (ncf->nak_src_nla_afi == AFI_IP6) ? 
						(char*)&ncf6->nak6_grp_nla_afi :
						(char*)&ncf->nak_grp_nla_afi );

/* OPT_NAK_LIST */
	struct pgm_opt_length* opt_len = (ncf->nak_src_nla_afi == AFI_IP6) ?
						(struct pgm_opt_length*)(ncf6 + 1) :
						(struct pgm_opt_length*)(ncf + 1);
	opt_len->opt_type	= PGM_OPT_LENGTH;
	opt_len->opt_length	= sizeof(struct pgm_opt_length);
	opt_len->opt_total_length = g_htons (	sizeof(struct pgm_opt_length) +
						sizeof(struct pgm_opt_header) +
						sizeof(struct pgm_opt_nak_list) +
						( (sqn_list->len-1) * sizeof(guint32) ) );
	struct pgm_opt_header* opt_header = (struct pgm_opt_header*)(opt_len + 1);
	opt_header->opt_type	= PGM_OPT_NAK_LIST | PGM_OPT_END;
	opt_header->opt_length	= sizeof(struct pgm_opt_header) +
				  sizeof(struct pgm_opt_nak_list) +
				  ( (sqn_list->len-1) * sizeof(guint32) );
	struct pgm_opt_nak_list* opt_nak_list = (struct pgm_opt_nak_list*)(opt_header + 1);
	opt_nak_list->opt_reserved = 0;
/* to network-order */
	for (unsigned i = 1; i < sqn_list->len; i++)
		opt_nak_list->opt_sqn[i-1] = g_htonl (sqn_list->sqn[i]);

        header->pgm_checksum    = 0;
        header->pgm_checksum	= pgm_csum_fold (pgm_csum_partial ((char*)header, tpdu_length, 0));

	const gssize sent = pgm_sendto (transport,
					FALSE,			/* not rate limited */
					TRUE,			/* with router alert */
					header,
					tpdu_length,
					(struct sockaddr*)&transport->send_gsr.gsr_group,
					pgm_sockaddr_len(&transport->send_gsr.gsr_group));
	if (-1 == sent && EAGAIN == errno)
		return FALSE;
	transport->cumulative_stats[PGM_PC_SOURCE_BYTES_SENT] += tpdu_length;
	return TRUE;
}

/* cancel any pending heartbeat SPM and schedule a new one
 *
 * on success, 0 is returned.  on error, -1 is returned, and errno set
 * appropriately.
 */

static
void
reset_heartbeat_spm (pgm_transport_t* transport)
{
	g_static_mutex_lock (&transport->mutex);

/* re-set spm timer */
	transport->spm_heartbeat_state = 1;
	transport->next_heartbeat_spm = pgm_time_update_now() + transport->spm_heartbeat_interval[transport->spm_heartbeat_state++];

/* prod recv thread if sleeping */
	if (pgm_time_after( transport->next_poll, transport->next_heartbeat_spm ))
		transport->next_poll = transport->next_heartbeat_spm;
	g_static_mutex_unlock (&transport->mutex);
}

/* state helper for resuming sends
 */
#define STATE(x)	(transport->pkt_dontwait_state.x)

/* send one PGM data packet, transmit window owned memory.
 *
 * on success, returns number of data bytes pushed into the transmit window and
 * attempted to send to the socket layer.  on non-blocking sockets, -1 is returned
 * if the packet sizes would exceed the current rate limit. on invalid arguments,
 * -EINVAL is returned.
 *
 * ! always returns successful if data is pushed into the transmit window, even if
 * sendto() double fails ¡  we don't want the application to try again as that is the
 * reliable transports role.
 */

static
GIOStatus
send_odata (
	pgm_transport_t* const		transport,
	struct pgm_sk_buff_t* const	skb,
	gsize*				bytes_written
	)
{
/* pre-conditions */
	g_assert (NULL != transport);
	g_assert (NULL != skb);
	g_assert (skb->len <= transport->max_tsdu);

	g_trace ("INFO","send_odata (transport:%p skb:%p bytes-written:%p)",
		(gpointer)transport, (gpointer)skb, (gpointer)bytes_written);

	const guint16 tsdu_length = skb->len;

/* continue if send would block */
	if (transport->is_apdu_eagain)
		goto retry_send;

/* add PGM header to skbuff */
	STATE(skb) = pgm_skb_get(skb);
	STATE(skb)->transport = transport;
	STATE(skb)->tstamp = pgm_time_update_now();
	STATE(skb)->data  = (guint8*)STATE(skb)->data - pgm_transport_pkt_offset(FALSE);
	STATE(skb)->len  += pgm_transport_pkt_offset(FALSE);

	STATE(skb)->pgm_header = (struct pgm_header*)STATE(skb)->data;
	STATE(skb)->pgm_data   = (struct pgm_data*)(STATE(skb)->pgm_header + 1);
	memcpy (STATE(skb)->pgm_header->pgm_gsi, &transport->tsi.gsi, sizeof(pgm_gsi_t));
	STATE(skb)->pgm_header->pgm_sport	= transport->tsi.sport;
	STATE(skb)->pgm_header->pgm_dport	= transport->dport;
	STATE(skb)->pgm_header->pgm_type        = PGM_ODATA;
        STATE(skb)->pgm_header->pgm_options     = 0;
        STATE(skb)->pgm_header->pgm_tsdu_length = g_htons (tsdu_length);

	g_static_rw_lock_writer_lock (&transport->window_lock);

/* ODATA */
        STATE(skb)->pgm_data->data_sqn		= g_htonl (pgm_txw_next_lead(transport->window));
        STATE(skb)->pgm_data->data_trail	= g_htonl (pgm_txw_trail(transport->window));

        STATE(skb)->pgm_header->pgm_checksum    = 0;
	const gsize pgm_header_len		= (guint8*)(STATE(skb)->pgm_data + 1) - (guint8*)STATE(skb)->pgm_header;
	const guint32 unfolded_header		= pgm_csum_partial (STATE(skb)->pgm_header, pgm_header_len, 0);
	STATE(unfolded_odata)			= pgm_csum_partial ((guint8*)(STATE(skb)->pgm_data + 1), tsdu_length, 0);
        STATE(skb)->pgm_header->pgm_checksum	= pgm_csum_fold (pgm_csum_block_add (unfolded_header, STATE(unfolded_odata), pgm_header_len));

/* add to transmit window */
	pgm_txw_add (transport->window, STATE(skb));

	gssize sent;
retry_send:
	sent = pgm_sendto (transport,
			   TRUE,			/* rate limited */
			   FALSE,			/* regular socket */
			   STATE(skb)->data,
			   STATE(skb)->len,
			   (struct sockaddr*)&transport->send_gsr.gsr_group,
			   pgm_sockaddr_len(&transport->send_gsr.gsr_group));
	if (sent < 0 && errno == EAGAIN) {
		transport->is_apdu_eagain = TRUE;
		return G_IO_STATUS_AGAIN;
	}

/* save unfolded odata for retransmissions */
	pgm_txw_set_unfolded_checksum (STATE(skb), STATE(unfolded_odata));

/* release window lock here in order to allow spms to lock mutex */
	g_static_rw_lock_writer_unlock (&transport->window_lock);

	transport->is_apdu_eagain = FALSE;
	reset_heartbeat_spm (transport);

	if ( sent == (gssize)STATE(skb)->len ) {
		transport->cumulative_stats[PGM_PC_SOURCE_DATA_BYTES_SENT] += tsdu_length;
		transport->cumulative_stats[PGM_PC_SOURCE_DATA_MSGS_SENT]  ++;
		transport->cumulative_stats[PGM_PC_SOURCE_BYTES_SENT]	   += STATE(skb)->len + transport->iphdr_len;
	}

/* check for end of transmission group */
	if (transport->use_proactive_parity) {
		const guint32 odata_sqn = g_ntohl (STATE(skb)->pgm_data->data_sqn);
		const guint32 tg_sqn_mask = 0xffffffff << transport->tg_sqn_shift;
		if (!((odata_sqn + 1) & ~tg_sqn_mask))
			pgm_schedule_proactive_nak (transport, odata_sqn & tg_sqn_mask);
	}

/* remove applications reference to skbuff */
	pgm_free_skb (STATE(skb));
	if (bytes_written)
		*bytes_written = tsdu_length;
	return G_IO_STATUS_NORMAL;
}

/* send one PGM original data packet, callee owned memory.
 *
 * on success, returns number of data bytes pushed into the transmit window and
 * attempted to send to the socket layer.  on non-blocking sockets, -1 is returned
 * if the packet sizes would exceed the current rate limit.
 */

static
GIOStatus
send_odata_copy (
	pgm_transport_t* const		transport,
	gconstpointer			tsdu,
	const gsize			tsdu_length,
	gsize*				bytes_written
	)
{
/* pre-conditions */
	g_assert (NULL != transport);
	g_assert (tsdu_length <= transport->max_tsdu);
	if (tsdu_length) g_assert (NULL != tsdu);

	g_trace ("INFO","send_odata_copy (transport:%p tsdu:%p tsdu_length:%" G_GSIZE_FORMAT " bytes-written:%p)",
		(gpointer)transport, tsdu, tsdu_length, (gpointer)bytes_written);

/* continue if blocked mid-apdu */
	if (transport->is_apdu_eagain)
		goto retry_send;

	g_static_rw_lock_writer_lock (&transport->window_lock);
	STATE(skb) = pgm_alloc_skb (transport->max_tpdu);
	STATE(skb)->transport = transport;
	STATE(skb)->tstamp = pgm_time_update_now();
	pgm_skb_put (STATE(skb), pgm_transport_pkt_offset (FALSE) + tsdu_length);

	STATE(skb)->pgm_header	= (struct pgm_header*)STATE(skb)->data;
	STATE(skb)->pgm_data	= (struct pgm_data*)(STATE(skb)->pgm_header + 1);
	memcpy (STATE(skb)->pgm_header->pgm_gsi, &transport->tsi.gsi, sizeof(pgm_gsi_t));
	STATE(skb)->pgm_header->pgm_sport	= transport->tsi.sport;
	STATE(skb)->pgm_header->pgm_dport	= transport->dport;
	STATE(skb)->pgm_header->pgm_type	= PGM_ODATA;
	STATE(skb)->pgm_header->pgm_options	= 0;
	STATE(skb)->pgm_header->pgm_tsdu_length = g_htons (tsdu_length);

/* ODATA */
	STATE(skb)->pgm_data->data_sqn		= g_htonl (pgm_txw_next_lead(transport->window));
	STATE(skb)->pgm_data->data_trail	= g_htonl (pgm_txw_trail(transport->window));

	STATE(skb)->pgm_header->pgm_checksum	= 0;
	const gsize pgm_header_len		= (guint8*)(STATE(skb)->pgm_data + 1) - (guint8*)STATE(skb)->pgm_header;
	const guint32 unfolded_header		= pgm_csum_partial (STATE(skb)->pgm_header, pgm_header_len, 0);
	STATE(unfolded_odata)			= pgm_csum_partial_copy (tsdu, (guint8*)(STATE(skb)->pgm_data + 1), tsdu_length, 0);
	STATE(skb)->pgm_header->pgm_checksum	= pgm_csum_fold (pgm_csum_block_add (unfolded_header, STATE(unfolded_odata), pgm_header_len));

/* add to transmit window */
	pgm_txw_add (transport->window, STATE(skb));

	gssize sent;
retry_send:
	sent = pgm_sendto (transport,
			   TRUE,			/* rate limited */
			   FALSE,			/* regular socket */
			   STATE(skb)->data,
			   STATE(skb)->len,
			   (struct sockaddr*)&transport->send_gsr.gsr_group,
			   pgm_sockaddr_len(&transport->send_gsr.gsr_group));
	if (sent < 0 && errno == EAGAIN) {
		transport->is_apdu_eagain = TRUE;
		return G_IO_STATUS_AGAIN;
	}

/* save unfolded odata for retransmissions */
	pgm_txw_set_unfolded_checksum (STATE(skb), STATE(unfolded_odata));

/* release txw lock here in order to allow spms to lock mutex */
	g_static_rw_lock_writer_unlock (&transport->window_lock);

	transport->is_apdu_eagain = FALSE;
	reset_heartbeat_spm (transport);

	if ( sent == (gssize)STATE(skb)->len ) {
		transport->cumulative_stats[PGM_PC_SOURCE_DATA_BYTES_SENT] += tsdu_length;
		transport->cumulative_stats[PGM_PC_SOURCE_DATA_MSGS_SENT]  ++;
		transport->cumulative_stats[PGM_PC_SOURCE_BYTES_SENT]	   += STATE(skb)->len + transport->iphdr_len;
	}

/* check for end of transmission group */
	if (transport->use_proactive_parity) {
		const guint32 odata_sqn = g_ntohl (STATE(skb)->pgm_data->data_sqn);
		const guint32 tg_sqn_mask = 0xffffffff << transport->tg_sqn_shift;
		if (!((odata_sqn + 1) & ~tg_sqn_mask))
			pgm_schedule_proactive_nak (transport, odata_sqn & tg_sqn_mask);
	}

/* return data payload length sent */
	if (bytes_written)
		*bytes_written = tsdu_length;
	return G_IO_STATUS_NORMAL;
}

/* send one PGM original data packet, callee owned scatter/gather io vector
 *
 *    ⎢ DATA₀ ⎢
 *    ⎢ DATA₁ ⎢ → send_odatav() →  ⎢ TSDU₀ ⎢ → libc
 *    ⎢   ⋮   ⎢
 *
 * on success, returns number of data bytes pushed into the transmit window and
 * attempted to send to the socket layer.  on non-blocking sockets, -1 is returned
 * if the packet sizes would exceed the current rate limit.
 */

static
GIOStatus
send_odatav (
	pgm_transport_t* const		transport,
	const struct pgm_iovec* const	vector,
	const guint			count,		/* number of items in vector */
	gsize*				bytes_written
	)
{
/* pre-conditions */
	g_assert (NULL != transport);
	g_assert (count <= PGM_MAX_FRAGMENTS);
	if (count) g_assert (NULL != vector);

	g_trace ("INFO","send_odatav (transport:%p vector:%p count:%u bytes-written:%p)",
		(gpointer)transport, (gconstpointer)vector, count, (gpointer)bytes_written);

	if (0 == count)
		return send_odata_copy (transport, NULL, 0, bytes_written);

/* continue if blocked on send */
	if (transport->is_apdu_eagain)
		goto retry_send;

	STATE(tsdu_length) = 0;
	for (unsigned i = 0; i < count; i++)
	{
#ifdef TRANSPORT_DEBUG
		if (vector[i].iov_len) {
			g_assert( vector[i].iov_base );
		}
#endif
		STATE(tsdu_length) += vector[i].iov_len;
	}
	g_return_val_if_fail (STATE(tsdu_length) <= transport->max_tsdu, G_IO_STATUS_ERROR);

	g_static_rw_lock_writer_lock (&transport->window_lock);
	STATE(skb) = pgm_alloc_skb (transport->max_tpdu);
	STATE(skb)->transport = transport;
	STATE(skb)->tstamp = pgm_time_update_now();
	pgm_skb_put (STATE(skb), pgm_transport_pkt_offset (FALSE) + STATE(tsdu_length));

	STATE(skb)->pgm_header  = (struct pgm_header*)STATE(skb)->data;
	STATE(skb)->pgm_data    = (struct pgm_data*)(STATE(skb)->pgm_header + 1);
	memcpy (STATE(skb)->pgm_header->pgm_gsi, &transport->tsi.gsi, sizeof(pgm_gsi_t));
	STATE(skb)->pgm_header->pgm_sport	= transport->tsi.sport;
	STATE(skb)->pgm_header->pgm_dport	= transport->dport;
	STATE(skb)->pgm_header->pgm_type	= PGM_ODATA;
	STATE(skb)->pgm_header->pgm_options	= 0;
	STATE(skb)->pgm_header->pgm_tsdu_length = g_htons (STATE(tsdu_length));

/* ODATA */
	STATE(skb)->pgm_data->data_sqn		= g_htonl (pgm_txw_next_lead(transport->window));
	STATE(skb)->pgm_data->data_trail	= g_htonl (pgm_txw_trail(transport->window));

	STATE(skb)->pgm_header->pgm_checksum	= 0;
	const gsize pgm_header_len		= (guint8*)(STATE(skb)->pgm_data + 1) - (guint8*)STATE(skb)->pgm_header;
	const guint32 unfolded_header		= pgm_csum_partial (STATE(skb)->pgm_header, pgm_header_len, 0);

/* unroll first iteration to make friendly branch prediction */
	guint8*	dst		= (guint8*)(STATE(skb)->pgm_data + 1);
	STATE(unfolded_odata)	= pgm_csum_partial_copy ((const guint8*)vector[0].iov_base, dst, vector[0].iov_len, 0);

/* iterate over one or more vector elements to perform scatter/gather checksum & copy */
	for (unsigned i = 1; i < count; i++) {
		dst += vector[i-1].iov_len;
		const guint32 unfolded_element = pgm_csum_partial_copy ((const guint8*)vector[i].iov_base, dst, vector[i].iov_len, 0);
		STATE(unfolded_odata) = pgm_csum_block_add (STATE(unfolded_odata), unfolded_element, vector[i-1].iov_len);
	}

	STATE(skb)->pgm_header->pgm_checksum	= pgm_csum_fold (pgm_csum_block_add (unfolded_header, STATE(unfolded_odata), pgm_header_len));

/* add to transmit window */
	pgm_txw_add (transport->window, STATE(skb));

	gssize sent;
retry_send:
	sent = pgm_sendto (transport,
			   TRUE,			/* rate limited */
			   FALSE,			/* regular socket */
			   STATE(skb)->data,
			   STATE(skb)->len,
			   (struct sockaddr*)&transport->send_gsr.gsr_group,
			   pgm_sockaddr_len(&transport->send_gsr.gsr_group));
	if (sent < 0 && errno == EAGAIN) {
		transport->is_apdu_eagain = TRUE;
		return G_IO_STATUS_AGAIN;
	}

/* save unfolded odata for retransmissions */
	pgm_txw_set_unfolded_checksum (STATE(skb), STATE(unfolded_odata));

/* release txw lock here in order to allow spms to lock mutex */
	g_static_rw_lock_writer_unlock (&transport->window_lock);

	transport->is_apdu_eagain = FALSE;
	reset_heartbeat_spm (transport);

	if ( sent == (gssize)STATE(skb)->len ) {
		transport->cumulative_stats[PGM_PC_SOURCE_DATA_BYTES_SENT] += STATE(tsdu_length);
		transport->cumulative_stats[PGM_PC_SOURCE_DATA_MSGS_SENT]  ++;
		transport->cumulative_stats[PGM_PC_SOURCE_BYTES_SENT]	   += STATE(skb)->len + transport->iphdr_len;
	}

/* check for end of transmission group */
	if (transport->use_proactive_parity) {
		const guint32 odata_sqn = g_ntohl (STATE(skb)->pgm_data->data_sqn);
		guint32 tg_sqn_mask = 0xffffffff << transport->tg_sqn_shift;
		if (!((odata_sqn + 1) & ~tg_sqn_mask))
			pgm_schedule_proactive_nak (transport, odata_sqn & tg_sqn_mask);
	}

/* return data payload length sent */
	if (bytes_written)
		*bytes_written = STATE(tsdu_length);
	return G_IO_STATUS_NORMAL;
}

/* send PGM original data, callee owned memory.  if larger than maximum TPDU
 * size will be fragmented.
 *
 * on success, returns number of data bytes pushed into the transmit window and
 * attempted to send to the socket layer.  on non-blocking sockets, -1 is returned
 * if the packet sizes would exceed the current rate limit.
 */

GIOStatus
pgm_send (
	pgm_transport_t* const		transport,
	gconstpointer			apdu,
	const gsize			apdu_length,
	gsize*				bytes_written
	)
{
	g_return_val_if_fail (NULL != transport, G_IO_STATUS_ERROR);
	g_return_val_if_fail (!transport->is_destroyed, G_IO_STATUS_ERROR);
	g_return_val_if_fail (apdu_length <= transport->max_apdu, G_IO_STATUS_ERROR);
	if (apdu_length) g_return_val_if_fail (NULL != apdu, G_IO_STATUS_ERROR);

	g_trace ("INFO","pgm_send (transport:%p apdu:%p apdu-length:%" G_GSIZE_FORMAT" bytes-written:%p)",
		(gpointer)transport, apdu, apdu_length, (gpointer)bytes_written);

/* pass on non-fragment calls */
	if (apdu_length < transport->max_tsdu)
		return send_odata_copy (transport, apdu, apdu_length, bytes_written);

	gsize bytes_sent	= 0;		/* counted at IP layer */
	guint packets_sent	= 0;		/* IP packets */
	gsize data_bytes_sent	= 0;

/* continue if blocked mid-apdu */
	if (transport->is_apdu_eagain)
		goto retry_send;

/* if non-blocking calculate total wire size and check rate limit */
	STATE(is_rate_limited) = FALSE;
	if (transport->is_nonblocking &&
	    transport->rate_control)
	{
		const gsize header_length = pgm_transport_pkt_offset (TRUE);
		gsize tpdu_length = 0;
		gsize offset_	  = 0;
		do {
			gsize tsdu_length = MIN( pgm_transport_max_tsdu (transport, TRUE), apdu_length - offset_ );
			tpdu_length += transport->iphdr_len + header_length + tsdu_length;
			offset_ += tsdu_length;
		} while (offset_ < apdu_length);

/* calculation includes one iphdr length already */
		if (!pgm_rate_check (transport->rate_control,
				     tpdu_length - transport->iphdr_len,
				     transport->is_nonblocking))
		{
			return G_IO_STATUS_AGAIN;
		}
		STATE(is_rate_limited) = TRUE;
	}

	STATE(data_bytes_offset) = 0;

	g_static_rw_lock_writer_lock (&transport->window_lock);
	STATE(first_sqn)	= pgm_txw_next_lead(transport->window);

	do {
/* retrieve packet storage from transmit window */
		gsize header_length = pgm_transport_pkt_offset (TRUE);
		STATE(tsdu_length) = MIN( pgm_transport_max_tsdu (transport, TRUE), apdu_length - STATE(data_bytes_offset) );

		STATE(skb) = pgm_alloc_skb (transport->max_tpdu);
		STATE(skb)->transport = transport;
		STATE(skb)->tstamp = pgm_time_update_now();
		pgm_skb_put (STATE(skb), header_length +  STATE(tsdu_length));

		STATE(skb)->pgm_header  = (struct pgm_header*)STATE(skb)->data;
		STATE(skb)->pgm_data    = (struct pgm_data*)(STATE(skb)->pgm_header + 1);
		memcpy (STATE(skb)->pgm_header->pgm_gsi, &transport->tsi.gsi, sizeof(pgm_gsi_t));
		STATE(skb)->pgm_header->pgm_sport	= transport->tsi.sport;
		STATE(skb)->pgm_header->pgm_dport	= transport->dport;
		STATE(skb)->pgm_header->pgm_type	= PGM_ODATA;
		STATE(skb)->pgm_header->pgm_options	= PGM_OPT_PRESENT;
		STATE(skb)->pgm_header->pgm_tsdu_length = g_htons (STATE(tsdu_length));

/* ODATA */
		STATE(skb)->pgm_data->data_sqn		= g_htonl (pgm_txw_next_lead(transport->window));
		STATE(skb)->pgm_data->data_trail	= g_htonl (pgm_txw_trail(transport->window));

/* OPT_LENGTH */
		struct pgm_opt_length* opt_len		= (struct pgm_opt_length*)(STATE(skb)->pgm_data + 1);
		opt_len->opt_type			= PGM_OPT_LENGTH;
		opt_len->opt_length			= sizeof(struct pgm_opt_length);
		opt_len->opt_total_length		= g_htons (	sizeof(struct pgm_opt_length) +
									sizeof(struct pgm_opt_header) +
									sizeof(struct pgm_opt_fragment) );
/* OPT_FRAGMENT */
		struct pgm_opt_header* opt_header	= (struct pgm_opt_header*)(opt_len + 1);
		opt_header->opt_type			= PGM_OPT_FRAGMENT | PGM_OPT_END;
		opt_header->opt_length			= sizeof(struct pgm_opt_header) +
						  	  sizeof(struct pgm_opt_fragment);
		STATE(skb)->pgm_opt_fragment			= (struct pgm_opt_fragment*)(opt_header + 1);
		STATE(skb)->pgm_opt_fragment->opt_reserved	= 0;
		STATE(skb)->pgm_opt_fragment->opt_sqn		= g_htonl (STATE(first_sqn));
		STATE(skb)->pgm_opt_fragment->opt_frag_off	= g_htonl (STATE(data_bytes_offset));
		STATE(skb)->pgm_opt_fragment->opt_frag_len	= g_htonl (apdu_length);

/* TODO: the assembly checksum & copy routine is faster than memcpy & pgm_cksum on >= opteron hardware */
		STATE(skb)->pgm_header->pgm_checksum	= 0;
		const gsize pgm_header_len		= (guint8*)(STATE(skb)->pgm_opt_fragment + 1) - (guint8*)STATE(skb)->pgm_header;
		const guint32 unfolded_header		= pgm_csum_partial (STATE(skb)->pgm_header, pgm_header_len, 0);
		STATE(unfolded_odata)			= pgm_csum_partial_copy ((const guint8*)apdu + STATE(data_bytes_offset), STATE(skb)->pgm_opt_fragment + 1, STATE(tsdu_length), 0);
		STATE(skb)->pgm_header->pgm_checksum	= pgm_csum_fold (pgm_csum_block_add (unfolded_header, STATE(unfolded_odata), pgm_header_len));

/* add to transmit window */
		pgm_txw_add (transport->window, STATE(skb));

		gssize sent;
retry_send:
		sent = pgm_sendto (transport,
				   !STATE(is_rate_limited),	/* rate limit on blocking */
				   FALSE,				/* regular socket */
				   STATE(skb)->data,
				   STATE(skb)->len,
				   (struct sockaddr*)&transport->send_gsr.gsr_group,
				   pgm_sockaddr_len(&transport->send_gsr.gsr_group));
		if (sent < 0 && errno == EAGAIN) {
			transport->is_apdu_eagain = TRUE;
			goto blocked;
		}

/* save unfolded odata for retransmissions */
		pgm_txw_set_unfolded_checksum (STATE(skb), STATE(unfolded_odata));

		if ( sent == (gssize)STATE(skb)->len ) {
			bytes_sent += STATE(skb)->len + transport->iphdr_len;	/* as counted at IP layer */
			packets_sent++;							/* IP packets */
			data_bytes_sent += STATE(tsdu_length);
		}

		STATE(data_bytes_offset) += STATE(tsdu_length);

/* check for end of transmission group */
		if (transport->use_proactive_parity) {
			const guint32 odata_sqn = g_ntohl (STATE(skb)->pgm_data->data_sqn);
			guint32 tg_sqn_mask = 0xffffffff << transport->tg_sqn_shift;
			if (!((odata_sqn + 1) & ~tg_sqn_mask))
				pgm_schedule_proactive_nak (transport, odata_sqn & tg_sqn_mask);
		}

	} while ( STATE(data_bytes_offset)  < apdu_length);
	g_assert( STATE(data_bytes_offset) == apdu_length );

/* release txw lock here in order to allow spms to lock mutex */
	g_static_rw_lock_writer_unlock (&transport->window_lock);

	transport->is_apdu_eagain = FALSE;
	reset_heartbeat_spm (transport);

	transport->cumulative_stats[PGM_PC_SOURCE_BYTES_SENT]      += bytes_sent;
	transport->cumulative_stats[PGM_PC_SOURCE_DATA_MSGS_SENT]  += packets_sent;
	transport->cumulative_stats[PGM_PC_SOURCE_DATA_BYTES_SENT] += data_bytes_sent;
	if (bytes_written)
		*bytes_written = apdu_length;
	return G_IO_STATUS_NORMAL;

blocked:
	if (bytes_sent) {
		reset_heartbeat_spm (transport);
		transport->cumulative_stats[PGM_PC_SOURCE_BYTES_SENT]      += bytes_sent;
		transport->cumulative_stats[PGM_PC_SOURCE_DATA_MSGS_SENT]  += packets_sent;
		transport->cumulative_stats[PGM_PC_SOURCE_DATA_BYTES_SENT] += data_bytes_sent;
	}
	return G_IO_STATUS_AGAIN;
}

/* send PGM original data, callee owned scatter/gather IO vector.  if larger than maximum TPDU
 * size will be fragmented.
 *
 * is_one_apdu = true:
 *
 *    ⎢ DATA₀ ⎢
 *    ⎢ DATA₁ ⎢ → pgm_sendv() →  ⎢ ⋯ TSDU₁ TSDU₀ ⎢ → libc
 *    ⎢   ⋮   ⎢
 *
 * is_one_apdu = false:
 *
 *    ⎢ APDU₀ ⎢                  ⎢ ⋯ TSDU₁,₀ TSDU₀,₀ ⎢
 *    ⎢ APDU₁ ⎢ → pgm_sendv() →  ⎢ ⋯ TSDU₁,₁ TSDU₀,₁ ⎢ → libc
 *    ⎢   ⋮   ⎢                  ⎢     ⋮       ⋮     ⎢
 *
 * on success, returns number of data bytes pushed into the transmit window and
 * attempted to send to the socket layer.  on non-blocking sockets, -1 is returned
 * if the packet sizes would exceed the current rate limit.
 */

GIOStatus
pgm_sendv (
	pgm_transport_t* const		transport,
	const struct pgm_iovec* const	vector,
	const guint			count,		/* number of items in vector */
	const gboolean			is_one_apdu,	/* true  = vector = apdu, false = vector::iov_base = apdu */
        gsize*                     	bytes_written
	)
{
	g_return_val_if_fail (NULL != transport, G_IO_STATUS_ERROR);
	g_return_val_if_fail (!transport->is_destroyed, G_IO_STATUS_ERROR);
	g_return_val_if_fail (count <= PGM_MAX_FRAGMENTS, G_IO_STATUS_ERROR);
	if (count) g_return_val_if_fail (NULL != vector, G_IO_STATUS_ERROR);

	g_trace ("INFO","pgm_sendv (transport:%p vector:%p count:%u is-one-apdu:%s bytes-written:%p)",
		(gpointer)transport,
		(gconstpointer)vector,
		count,
		is_one_apdu ? "TRUE" : "FALSE",
		(gpointer)bytes_written);

/* pass on zero length as cannot count vector lengths */
	if (count == 0)
		return send_odata_copy (transport, NULL, count, bytes_written);

	gsize bytes_sent	= 0;
	guint packets_sent	= 0;
	gsize data_bytes_sent	= 0;

/* continue if blocked mid-apdu */
	if (transport->is_apdu_eagain) {
		if (is_one_apdu) {
			if (STATE(apdu_length) < transport->max_tsdu)
				return send_odatav (transport, vector, count, bytes_written);
			else
				goto retry_one_apdu_send;
		} else {
			goto retry_send;
		}
	}

/* calculate (total) APDU length */
	STATE(apdu_length)	= 0;
	for (unsigned i = 0; i < count; i++)
	{
#ifdef TRANSPORT_DEBUG
		if (vector[i].iov_len) {
			g_assert( vector[i].iov_base );
		}
#endif
		if (!is_one_apdu) {
			g_return_val_if_fail (vector[i].iov_len <= transport->max_apdu, G_IO_STATUS_ERROR);
		}
		STATE(apdu_length) += vector[i].iov_len;
	}

/* pass on non-fragment calls */
	if (is_one_apdu) {
		if (STATE(apdu_length) < transport->max_tsdu) {
			return send_odatav (transport, vector, count, bytes_written);
		} else {
			g_return_val_if_fail (STATE(apdu_length) <= transport->max_apdu, G_IO_STATUS_ERROR);
		}
	}

/* if non-blocking calculate total wire size and check rate limit */
	STATE(is_rate_limited) = FALSE;
	if (transport->is_nonblocking &&
	    transport->rate_control)
        {
		const gsize header_length = pgm_transport_pkt_offset (TRUE);
                gsize tpdu_length = 0;
		guint offset_	  = 0;
		do {
			gsize tsdu_length = MIN( pgm_transport_max_tsdu (transport, TRUE), STATE(apdu_length) - offset_ );
			tpdu_length += transport->iphdr_len + header_length + tsdu_length;
			offset_     += tsdu_length;
		} while (offset_ < STATE(apdu_length));

/* calculation includes one iphdr length already */
                if (!pgm_rate_check (transport->rate_control,
				     tpdu_length - transport->iphdr_len,
				     transport->is_nonblocking))
		{
			return G_IO_STATUS_AGAIN;
		}
		STATE(is_rate_limited) = TRUE;
        }

/* non-fragmented packets can be forwarded onto basic send() */
	if (!is_one_apdu)
	{
		for (STATE(data_pkt_offset) = 0; STATE(data_pkt_offset) < count; STATE(data_pkt_offset)++)
		{
			gsize wrote_bytes;
			GIOStatus status;
retry_send:
			status = pgm_send (transport,
					   vector[STATE(data_pkt_offset)].iov_base,
					   vector[STATE(data_pkt_offset)].iov_len,
					   &wrote_bytes);
			if (G_IO_STATUS_AGAIN == status) {
				transport->is_apdu_eagain = TRUE;
				return status;
			} else if (G_IO_STATUS_NORMAL != status)
				return status;
			data_bytes_sent += wrote_bytes;
		}

		transport->is_apdu_eagain = FALSE;
		if (bytes_written)
			*bytes_written = data_bytes_sent;
		return G_IO_STATUS_NORMAL;
	}

	STATE(data_bytes_offset)	= 0;
	STATE(vector_index)		= 0;
	STATE(vector_offset)		= 0;

	g_static_rw_lock_writer_lock (&transport->window_lock);
	STATE(first_sqn)		= pgm_txw_next_lead(transport->window);

	do {
/* retrieve packet storage from transmit window */
		gsize header_length = pgm_transport_pkt_offset (TRUE);
		STATE(tsdu_length) = MIN( pgm_transport_max_tsdu (transport, TRUE), STATE(apdu_length) - STATE(data_bytes_offset) );
		STATE(skb) = pgm_alloc_skb (transport->max_tpdu);
		STATE(skb)->transport = transport;
		STATE(skb)->tstamp = pgm_time_update_now();
		pgm_skb_put (STATE(skb), header_length + STATE(tsdu_length));

		STATE(skb)->pgm_header  = (struct pgm_header*)STATE(skb)->data;
		STATE(skb)->pgm_data    = (struct pgm_data*)(STATE(skb)->pgm_header + 1);
		memcpy (STATE(skb)->pgm_header->pgm_gsi, &transport->tsi.gsi, sizeof(pgm_gsi_t));
		STATE(skb)->pgm_header->pgm_sport	= transport->tsi.sport;
		STATE(skb)->pgm_header->pgm_dport	= transport->dport;
		STATE(skb)->pgm_header->pgm_type	= PGM_ODATA;
		STATE(skb)->pgm_header->pgm_options	= PGM_OPT_PRESENT;
		STATE(skb)->pgm_header->pgm_tsdu_length = g_htons (STATE(tsdu_length));

/* ODATA */
		STATE(skb)->pgm_data->data_sqn		= g_htonl (pgm_txw_next_lead(transport->window));
		STATE(skb)->pgm_data->data_trail	= g_htonl (pgm_txw_trail(transport->window));

/* OPT_LENGTH */
		struct pgm_opt_length* opt_len		= (struct pgm_opt_length*)(STATE(skb)->pgm_data + 1);
		opt_len->opt_type			= PGM_OPT_LENGTH;
		opt_len->opt_length			= sizeof(struct pgm_opt_length);
		opt_len->opt_total_length		= g_htons (	sizeof(struct pgm_opt_length) +
									sizeof(struct pgm_opt_header) +
									sizeof(struct pgm_opt_fragment) );
/* OPT_FRAGMENT */
		struct pgm_opt_header* opt_header	= (struct pgm_opt_header*)(opt_len + 1);
		opt_header->opt_type			= PGM_OPT_FRAGMENT | PGM_OPT_END;
		opt_header->opt_length			= sizeof(struct pgm_opt_header) +
							  sizeof(struct pgm_opt_fragment);
		STATE(skb)->pgm_opt_fragment			= (struct pgm_opt_fragment*)(opt_header + 1);
		STATE(skb)->pgm_opt_fragment->opt_reserved	= 0;
		STATE(skb)->pgm_opt_fragment->opt_sqn		= g_htonl (STATE(first_sqn));
		STATE(skb)->pgm_opt_fragment->opt_frag_off	= g_htonl (STATE(data_bytes_offset));
		STATE(skb)->pgm_opt_fragment->opt_frag_len	= g_htonl (STATE(apdu_length));

/* checksum & copy */
		STATE(skb)->pgm_header->pgm_checksum	= 0;
		const gsize pgm_header_len		= (guint8*)(STATE(skb)->pgm_opt_fragment + 1) - (guint8*)STATE(skb)->pgm_header;
		const guint32 unfolded_header		= pgm_csum_partial (STATE(skb)->pgm_header, pgm_header_len, 0);

/* iterate over one or more vector elements to perform scatter/gather checksum & copy
 *
 * STATE(vector_index)	- index into application scatter/gather vector
 * STATE(vector_offset) - current offset into current vector element
 * STATE(unfolded_odata)- checksum accumulator
 */
		const guint8* src	= (const guint8*)vector[STATE(vector_index)].iov_base + STATE(vector_offset);
		guint8* dst		= (guint8*)(STATE(skb)->pgm_opt_fragment + 1);
		gsize src_length	= vector[STATE(vector_index)].iov_len - STATE(vector_offset);
		gsize dst_length	= 0;
		gsize copy_length	= MIN( STATE(tsdu_length), src_length );
		STATE(unfolded_odata)	= pgm_csum_partial_copy (src, dst, copy_length, 0);

		for(;;)
		{
			if (copy_length == src_length) {
/* application packet complete */
				STATE(vector_index)++;
				STATE(vector_offset) = 0;
			} else {
/* data still remaining */
				STATE(vector_offset) += copy_length;
			}

			dst_length += copy_length;

/* transport packet complete */
			if (dst_length == STATE(tsdu_length))
				break;

			src		= (const guint8*)vector[STATE(vector_index)].iov_base + STATE(vector_offset);
			dst	       += copy_length;
			src_length	= vector[STATE(vector_index)].iov_len - STATE(vector_offset);
			copy_length	= MIN( STATE(tsdu_length) - dst_length, src_length );
			const guint32 unfolded_element = pgm_csum_partial_copy (src, dst, copy_length, 0);
			STATE(unfolded_odata) = pgm_csum_block_add (STATE(unfolded_odata), unfolded_element, dst_length);
		}

		STATE(skb)->pgm_header->pgm_checksum = pgm_csum_fold (pgm_csum_block_add (unfolded_header, STATE(unfolded_odata), pgm_header_len));

/* add to transmit window */
		pgm_txw_add (transport->window, STATE(skb));

		gssize sent;
retry_one_apdu_send:
		sent = pgm_sendto (transport,
				   !STATE(is_rate_limited),	/* rate limited on blocking */
				   FALSE,				/* regular socket */
				   STATE(skb)->data,
				   STATE(skb)->len,
				   (struct sockaddr*)&transport->send_gsr.gsr_group,
				   pgm_sockaddr_len(&transport->send_gsr.gsr_group));
		if (sent < 0 && errno == EAGAIN) {
			transport->is_apdu_eagain = TRUE;
			goto blocked;
		}

/* save unfolded odata for retransmissions */
		pgm_txw_set_unfolded_checksum (STATE(skb), STATE(unfolded_odata));

		if ( sent == (gssize)STATE(skb)->len ) {
			bytes_sent += STATE(skb)->len + transport->iphdr_len;	/* as counted at IP layer */
			packets_sent++;							/* IP packets */
			data_bytes_sent += STATE(tsdu_length);
		}

		STATE(data_bytes_offset) += STATE(tsdu_length);

/* check for end of transmission group */
		if (transport->use_proactive_parity) {
			const guint32 odata_sqn = g_ntohl (STATE(skb)->pgm_data->data_sqn);
			guint32 tg_sqn_mask = 0xffffffff << transport->tg_sqn_shift;
			if (!((odata_sqn + 1) & ~tg_sqn_mask))
				pgm_schedule_proactive_nak (transport, odata_sqn & tg_sqn_mask);
		}

	} while ( STATE(data_bytes_offset)  < STATE(apdu_length) );
	g_assert( STATE(data_bytes_offset) == STATE(apdu_length) );

/* release txw lock here in order to allow spms to lock mutex */
	g_static_rw_lock_writer_unlock (&transport->window_lock);

	transport->is_apdu_eagain = FALSE;
	reset_heartbeat_spm (transport);

	transport->cumulative_stats[PGM_PC_SOURCE_BYTES_SENT]      += bytes_sent;
	transport->cumulative_stats[PGM_PC_SOURCE_DATA_MSGS_SENT]  += packets_sent;
	transport->cumulative_stats[PGM_PC_SOURCE_DATA_BYTES_SENT] += data_bytes_sent;
	if (bytes_written)
		*bytes_written = STATE(apdu_length);
	return G_IO_STATUS_NORMAL;

blocked:
	if (bytes_sent) {
		reset_heartbeat_spm (transport);
		transport->cumulative_stats[PGM_PC_SOURCE_BYTES_SENT]      += bytes_sent;
		transport->cumulative_stats[PGM_PC_SOURCE_DATA_MSGS_SENT]  += packets_sent;
		transport->cumulative_stats[PGM_PC_SOURCE_DATA_BYTES_SENT] += data_bytes_sent;
	}
	return G_IO_STATUS_AGAIN;
}

/* send PGM original data, transmit window owned scatter/gather IO vector.
 *
 *    ⎢ TSDU₀ ⎢
 *    ⎢ TSDU₁ ⎢ → pgm_send_skbv() →  ⎢ ⋯ TSDU₁ TSDU₀ ⎢ → libc
 *    ⎢   ⋮   ⎢
 *
 * on success, returns number of data bytes pushed into the transmit window and
 * attempted to send to the socket layer.  on non-blocking sockets, -1 is returned
 * if the packet sizes would exceed the current rate limit.
 */

GIOStatus
pgm_send_skbv (
	pgm_transport_t* const		transport,
	struct pgm_sk_buff_t** const	vector,		/* array of skb pointers vs. array of skbs */
	const guint			count,
	const gboolean			is_one_apdu,	/* true: vector = apdu, false: vector::iov_base = apdu */
	gsize*				bytes_written
	)
{
	g_return_val_if_fail (NULL != transport, G_IO_STATUS_ERROR);
	g_return_val_if_fail (!transport->is_destroyed, G_IO_STATUS_ERROR);
	g_return_val_if_fail (count <= PGM_MAX_FRAGMENTS, G_IO_STATUS_ERROR);
	if (count) g_return_val_if_fail (NULL != vector, G_IO_STATUS_ERROR);

	g_trace ("INFO","pgm_send_skbv (transport:%p vector:%p count:%u is-one-apdu:%s bytes-written:%p)",
		(gpointer)transport,
		(gpointer)vector,
		count,
		is_one_apdu ? "TRUE" : "FALSE",
		(gpointer)bytes_written);

/* pass on zero length as cannot count vector lengths */
	if (0 == count)
		return send_odata_copy (transport, NULL, count, bytes_written);
	if (1 == count)
		return send_odata (transport, vector[0], bytes_written);

	gsize bytes_sent	= 0;
	guint packets_sent	= 0;
	gsize data_bytes_sent	= 0;

/* continue if blocked mid-apdu */
	if (transport->is_apdu_eagain)
		goto retry_send;

	STATE(is_rate_limited) = FALSE;
	if (transport->is_nonblocking &&
	    transport->rate_control)
	{
		gsize total_tpdu_length = 0;
		for (guint i = 0; i < count; i++)
			total_tpdu_length += transport->iphdr_len + pgm_transport_pkt_offset (is_one_apdu) + vector[i]->len;

/* calculation includes one iphdr length already */
		if (!pgm_rate_check (transport->rate_control,
				     total_tpdu_length - transport->iphdr_len,
				     transport->is_nonblocking))
		{
			return G_IO_STATUS_AGAIN;
		}
		STATE(is_rate_limited) = TRUE;
	}

	g_static_rw_lock_writer_lock (&transport->window_lock);

	if (is_one_apdu)
	{
		STATE(apdu_length)	= 0;
		STATE(first_sqn)	= pgm_txw_next_lead(transport->window);
		for (guint i = 0; i < count; i++) {
			g_return_val_if_fail (vector[i]->len <= transport->max_tsdu_fragment, G_IO_STATUS_ERROR);
			STATE(apdu_length) += vector[i]->len;
		}
		g_return_val_if_fail (STATE(apdu_length) <= transport->max_apdu, G_IO_STATUS_ERROR);
	}

	for (STATE(vector_index) = 0; STATE(vector_index) < count; STATE(vector_index)++)
	{
		STATE(tsdu_length) = vector[STATE(vector_index)]->len;
		
		STATE(skb) = pgm_skb_get(vector[STATE(vector_index)]);
		STATE(skb)->transport = transport;
		STATE(skb)->tstamp = pgm_time_update_now();
		STATE(skb)->data  = (guint8*)STATE(skb)->data - pgm_transport_pkt_offset(is_one_apdu);
		STATE(skb)->len  += pgm_transport_pkt_offset(is_one_apdu);

		STATE(skb)->pgm_header = (struct pgm_header*)STATE(skb)->data;
		STATE(skb)->pgm_data   = (struct pgm_data*)(STATE(skb)->pgm_header + 1);
		memcpy (STATE(skb)->pgm_header->pgm_gsi, &transport->tsi.gsi, sizeof(pgm_gsi_t));
		STATE(skb)->pgm_header->pgm_sport	= transport->tsi.sport;
		STATE(skb)->pgm_header->pgm_dport	= transport->dport;
		STATE(skb)->pgm_header->pgm_type	= PGM_ODATA;
		STATE(skb)->pgm_header->pgm_options	= is_one_apdu ? PGM_OPT_PRESENT : 0;
		STATE(skb)->pgm_header->pgm_tsdu_length = g_htons (STATE(tsdu_length));

/* ODATA */
		STATE(skb)->pgm_data->data_sqn		= g_htonl (pgm_txw_next_lead(transport->window));
		STATE(skb)->pgm_data->data_trail	= g_htonl (pgm_txw_trail(transport->window));

		gpointer dst = NULL;

		if (is_one_apdu)
		{
/* OPT_LENGTH */
			struct pgm_opt_length* opt_len		= (struct pgm_opt_length*)(STATE(skb)->pgm_data + 1);
			opt_len->opt_type			= PGM_OPT_LENGTH;
			opt_len->opt_length			= sizeof(struct pgm_opt_length);
			opt_len->opt_total_length		= g_htons (	sizeof(struct pgm_opt_length) +
										sizeof(struct pgm_opt_header) +
										sizeof(struct pgm_opt_fragment) );
/* OPT_FRAGMENT */
			struct pgm_opt_header* opt_header	= (struct pgm_opt_header*)(opt_len + 1);
			opt_header->opt_type			= PGM_OPT_FRAGMENT | PGM_OPT_END;
			opt_header->opt_length			= sizeof(struct pgm_opt_header) +
								  sizeof(struct pgm_opt_fragment);
			STATE(skb)->pgm_opt_fragment			= (struct pgm_opt_fragment*)(opt_header + 1);
			STATE(skb)->pgm_opt_fragment->opt_reserved	= 0;
			STATE(skb)->pgm_opt_fragment->opt_sqn		= g_htonl (STATE(first_sqn));
			STATE(skb)->pgm_opt_fragment->opt_frag_off	= g_htonl (STATE(data_bytes_offset));
			STATE(skb)->pgm_opt_fragment->opt_frag_len	= g_htonl (STATE(apdu_length));

			dst = STATE(skb)->pgm_opt_fragment + 1;
		}
		else
		{
			dst = STATE(skb)->pgm_data + 1;
		}

/* TODO: the assembly checksum & copy routine is faster than memcpy & pgm_cksum on >= opteron hardware */
		STATE(skb)->pgm_header->pgm_checksum	= 0;
		const gsize pgm_header_len		= (guint8*)dst - (guint8*)STATE(skb)->pgm_header;
		const guint32 unfolded_header		= pgm_csum_partial (STATE(skb)->pgm_header, pgm_header_len, 0);
		STATE(unfolded_odata)			= pgm_csum_partial ((guint8*)(STATE(skb)->pgm_opt_fragment + 1), STATE(tsdu_length), 0);
		STATE(skb)->pgm_header->pgm_checksum	= pgm_csum_fold (pgm_csum_block_add (unfolded_header, STATE(unfolded_odata), pgm_header_len));

/* add to transmit window */
		pgm_txw_add (transport->window, STATE(skb));
		gssize sent;
retry_send:
		sent = pgm_sendto (transport,
				   !STATE(is_rate_limited),	/* rate limited on blocking */
				    FALSE,				/* regular socket */
				    STATE(skb)->data,
				    STATE(skb)->len,
				    (struct sockaddr*)&transport->send_gsr.gsr_group,
				    pgm_sockaddr_len(&transport->send_gsr.gsr_group));
		if (sent < 0 && errno == EAGAIN) {
			transport->is_apdu_eagain = TRUE;
			goto blocked;
		}

/* save unfolded odata for retransmissions */
		pgm_txw_set_unfolded_checksum (STATE(skb), STATE(unfolded_odata));

		if (sent == (gssize)STATE(skb)->len) {
			bytes_sent += STATE(skb)->len + transport->iphdr_len;	/* as counted at IP layer */
			packets_sent++;							/* IP packets */
			data_bytes_sent += STATE(tsdu_length);
		}

		pgm_free_skb (STATE(skb));
		STATE(data_bytes_offset) += STATE(tsdu_length);

/* check for end of transmission group */
		if (transport->use_proactive_parity) {
			const guint32 odata_sqn = g_ntohl (STATE(skb)->pgm_data->data_sqn);
			guint32 tg_sqn_mask = 0xffffffff << transport->tg_sqn_shift;
			if (!((odata_sqn + 1) & ~tg_sqn_mask))
				pgm_schedule_proactive_nak (transport, odata_sqn & tg_sqn_mask);
		}

	}
#ifdef TRANSPORT_DEBUG
	if (is_one_apdu)
	{
		g_assert( STATE(data_bytes_offset) == STATE(apdu_length) );
	}
#endif

/* release txw lock here in order to allow spms to lock mutex */
	g_static_rw_lock_writer_unlock (&transport->window_lock);

	transport->is_apdu_eagain = FALSE;
	reset_heartbeat_spm (transport);

	transport->cumulative_stats[PGM_PC_SOURCE_BYTES_SENT]      += bytes_sent;
	transport->cumulative_stats[PGM_PC_SOURCE_DATA_MSGS_SENT]  += packets_sent;
	transport->cumulative_stats[PGM_PC_SOURCE_DATA_BYTES_SENT] += data_bytes_sent;
	if (bytes_written)
		*bytes_written = data_bytes_sent;
	return G_IO_STATUS_NORMAL;

blocked:
	if (bytes_sent) {
		reset_heartbeat_spm (transport);
		transport->cumulative_stats[PGM_PC_SOURCE_BYTES_SENT]      += bytes_sent;
		transport->cumulative_stats[PGM_PC_SOURCE_DATA_MSGS_SENT]  += packets_sent;
		transport->cumulative_stats[PGM_PC_SOURCE_DATA_BYTES_SENT] += data_bytes_sent;
	}
	return G_IO_STATUS_AGAIN;
}

/* cleanup resuming send state helper 
 */
#undef STATE

/* send repair packet.
 *
 * on success, TRUE is returned.  on error, FALSE is returned.
 */

static
gboolean
send_rdata (
	pgm_transport_t*	transport,
	struct pgm_sk_buff_t*	skb
	)
{
/* pre-conditions */
	g_assert (NULL != transport);
	g_assert (NULL != skb);

/* update previous odata/rdata contents */
	struct pgm_header* header	= skb->pgm_header;
	struct pgm_data* rdata		= skb->pgm_data;
	header->pgm_type		= PGM_RDATA;
/* RDATA */
        rdata->data_trail		= g_htonl (pgm_txw_trail(transport->window));

        header->pgm_checksum		= 0;
	const gsize pgm_header_len	= skb->len - g_ntohs(header->pgm_tsdu_length);
	guint32 unfolded_header		= pgm_csum_partial (header, pgm_header_len, 0);
	guint32 unfolded_odata		= pgm_txw_get_unfolded_checksum (skb);
	header->pgm_checksum		= pgm_csum_fold (pgm_csum_block_add (unfolded_header, unfolded_odata, pgm_header_len));

	const gssize sent = pgm_sendto (transport,
					TRUE,			/* rate limited */
					TRUE,			/* with router alert */
					header,
					skb->len,
					(struct sockaddr*)&transport->send_gsr.gsr_group,
					pgm_sockaddr_len(&transport->send_gsr.gsr_group));
/* re-save unfolded payload for further retransmissions */
	pgm_txw_set_unfolded_checksum (skb, unfolded_odata);

	if (-1 == sent && EAGAIN == errno)
		return FALSE;

/* re-set spm timer: we are already in the timer thread, no need to prod timers
 */
	g_static_mutex_lock (&transport->mutex);
	transport->spm_heartbeat_state = 1;
	transport->next_heartbeat_spm = pgm_time_update_now() + transport->spm_heartbeat_interval[transport->spm_heartbeat_state++];
	g_static_mutex_unlock (&transport->mutex);

	transport->cumulative_stats[PGM_PC_SOURCE_SELECTIVE_BYTES_RETRANSMITTED] += g_ntohs(header->pgm_tsdu_length);
	transport->cumulative_stats[PGM_PC_SOURCE_SELECTIVE_MSGS_RETRANSMITTED]++;	/* impossible to determine APDU count */
	transport->cumulative_stats[PGM_PC_SOURCE_BYTES_SENT] += skb->len + transport->iphdr_len;
	return TRUE;
}

/* eof */