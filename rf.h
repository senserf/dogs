/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef	__pg_rf_h
#define	__pg_rf_h

//
// For CC1350 (LAUNCHPAD)
//

#include "sysio.h"
#include "cc1350.h"
#include "phys_cc1350.h"
#include "ossi.h"

// ============================================================================
// Packet offsets and casts
// ============================================================================

// Note that CC1350_MAXPLEN = 250, we can play with this
#define	MAX_PACKET_LENGTH		64
// Trailer length
#define	PKT_FRAME_TRAIL			2
// PHY header
#define	PKT_FRAME_PHDR			2
// OSS frame
#define	PKT_FRAME_OSS			sizeof (oss_hdr_t)
// Full frame
#define	PKT_FRAME_ALL			(PKT_FRAME_PHDR + PKT_FRAME_OSS +\
						PKT_FRAME_TRAIL)
// OSS header offset
#define	pkt_osshdr(p)			((oss_hdr_t*)(((byte*)(p)) + \
						PKT_FRAME_PHDR))
// Payload offset
#define	pkt_payload(p)			(((address)(p)) + ((PKT_FRAME_PHDR + \
						PKT_FRAME_OSS)/2))
#define	GROUP_ID		((word)(host_id >> 16))

// ============================================================================
// ACK codes
// ============================================================================
#define	ACK_OK			0
#define	ACK_FMT			1
#define	ACK_LENGTH		2
#define	ACK_PARAM		3
#define	ACK_COMMAND		4
#define	ACK_ISOFF		6
#define	ACK_BUSY		7
#define	ACK_NORES		8
#define	ACK_CONFIG		9
#define	ACK_VOID		10

#define	ACK_AP_BAD		129
#define	ACK_AP_FMT		130
#define	ACK_AP_TOOLONG		131

#define	set_lbt(pkt,boo)	set_pxopts (pkt, 0, boo, RADIO_DEFAULT_POWER)

#if (RADIO_OPTIONS & RADIO_OPTION_PXOPTIONS)
#define	tcv_endpx(msg,boo)	do { set_lbt (msg, boo); tcv_endp (msg); } \
					while (0)
#else
#define	tcv_endpx(msg,boo)	tcv_endp (msg)
#endif

#endif
