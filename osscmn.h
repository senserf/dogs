/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef	__pg_osscmn_h
#define	__pg_osscmn_h

//+++ osscmn.cc

#include "rf.h"
#include "ossi.h"
#include "tcvphys.h"
#include "plug_null.h"

// ============================================================================

#define	RF_MONITOR_INTERVAL	2048
// Note: 10/1500 seems to work quite fine
#define	RF_ON_INTERVAL		10		// 50
#define	RF_OFF_INTERVAL		1500		// 990 (adds to 1000)
#define	RF_WCLEAR_INTERVAL	512
#define	RF_MONITOR_CD		15		// Amounts to 30 seconds
#define	RF_WAKE_COUNT		768
#define	RF_WAKE_SPACE		3

// ============================================================================

// Message codes >= 128 are special
#define MESSAGE_CODE_SBLOCK		128	// Streaming block
#define MESSAGE_CODE_ETRAIN		129	// End of train
#define	MESSAGE_CODE_STRACK		128 	// Train ACK (app -> tag)

// 12 x 4 = 48 bytes; this is the payload size of a streaming packet
#define	STRM_NCODES		12
// Maximum number of packets in a train (roughly, one packet = 64 bytes, 156
// blocks per 10 K), make it 128
#define	STRM_TRAIN_LENGTH	64
// Maximum number of stored packets
#define	STRM_MAX_QUEUED		128
// Maximum block offset; this must be derived from the bitmap size for the Peg
// which must be a power of two
#define	STRM_MAP_SIZE		256
#define	STRM_MAP_MASK		(STRM_MAP_SIZE - 1)
// The block span is equal to 8 * MAP_SIZE - 7; this is the worst case number
// of blocks from the actual (unaligned) offset accommodated in the map
#define	STRM_MAX_BLOCKSPAN	(8 * STRM_MAP_SIZE - 7)
// TX space between cars; make it a parameter? Should be larger than the
// intrinsic space of the driver with LBT off
#define	STRM_CAR_SPACE		5
// Space between EOT packets
#define	STRM_MIN_TRAIN_SPACE	20
#define	STRM_MAX_TRAIN_SPACE	512
// Max payload of ACK packet
#define	STRM_MAX_ACKPAY		58

#ifdef __SMURPH__
#undef	STRM_TRAIN_LENGTH	
#undef	STRM_MAX_QUEUED
#undef	STRM_MAP_SIZE
#undef	STRM_TRAIN_SPACE
#undef	STRM_MAX_ACKPAY
#define	STRM_TRAIN_LENGTH	8
#define	STRM_MAX_QUEUED		16
#define	STRM_MAP_SIZE		16
#define	STRM_TRAIN_SPACE	128
#define	STRM_MAX_ACKPAY		16
#endif

// Train sender status
#define	STRM_TSSTAT_NONE	0
#define	STRM_TSSTAT_WDAT	1
#define STRM_TSSTAT_WACK	2

typedef	struct strblk_t strblk_t;

struct strblk_t {
//
// Queued block awaiting transmission in a car
//
	strblk_t	*next;		// We link them
	lword 		bn;		// Block number
	lword		block [STRM_NCODES];
};

// ============================================================================

extern byte	LastRef;
extern sint	RFC;

void osscmn_init ();
address osscmn_xpkt (byte, byte, word);
void osscmn_xack (byte, word);

// Provided by the application
void handle_rf_packet (byte, byte, const address, word);

#define toggle(a)	((a) = 1 - (a))

#endif
