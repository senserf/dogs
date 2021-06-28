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

#define	MAX_PACKET_LENGTH		CC1350_MAXPLEN
// NetId + HostId + CRC
#define	RFPFRAME			6
// Offset to OSS header
#define	RFPHDOFF			4
// RFPHDOFF + OSS header
#define	OSSFRAME			(RFPHDOFF + sizeof (oss_hdr_t))
// Minimum length of an OSS packet (header + at least one parameter)
#define	OSSMINPL			(RFPFRAME + sizeof (oss_hdr_t) + 2)

// RF packet offsets to OSS info: oss hdr
#define	osshdr(p)		((oss_hdr_t*)(((byte*)(p)) + RFPHDOFF))
// First word past the header
#define	osspar(p)		(((address)(p)) + (OSSFRAME/2))

#define	NODE_ID			((word)host_id)

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
#define	ACK_VOID		9

#define	ACK_AP_BAD		129
#define	ACK_AP_FMT		130
#define	ACK_AP_TOOLONG		131

#endif
