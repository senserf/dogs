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
// NHId + CRC (there is just the network Id
#define	RFPFRAME			4
// Offset to OSS header
#define	RFPHDOFF			2
// RFPHDOFF + OSS header
#define	OSSFRAME			(RFPHDOFF + sizeof (oss_hdr_t))
// Minimum length of an OSS packet (header + at least one parameter)
#define	OSSMINPL			(RFPFRAME + sizeof (oss_hdr_t) + 2)
// Maximum length of a packet payload
#define	MAX_PAYLOAD_LENGTH		(MAX_PACKET_LENGTH - RFPFRAME - \
						sizeof (oss_hdr_t))

// RF packet offsets to OSS info: oss hdr
#define	osshdr(p)		((oss_hdr_t*)(((byte*)(p)) + RFPHDOFF))
// First word past the header
#define	osspar(p)		(((address)(p)) + (OSSFRAME/2))

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
#define	tcv_endpx(msg,boo)	do { set_lbt (msg, boo); tcv_endp (msg); } \
					while (0)
#endif
