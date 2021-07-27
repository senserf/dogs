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


// RF params (mutable)
#define	WOR_CYCLE		1024			// 1 second
#define	RADIO_LINGER		(5 * 1024)		// 5 seconds
#define	WOR_RSS			20
#define	WOR_PQI			YES

#define	RADIO_MODE_HIBERNATE	3
#define	RADIO_MODE_ON		2
#define	RADIO_MODE_WOR		1
#define RADIO_MODE_OFF		0

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
#define	STRM_MAX_OFFSET		(STRM_MAX_BLOCKSPAN - 1)
// TX space between cars; make it a parameter? Should be larger than the
// intrinsic space of the driver with LBT off
#define	STRM_CAR_SPACE		5
// Space between EOT packets
#define	STRM_TRAIN_SPACE	10
// Max payload of ACK packet
#define	STRM_MAX_ACKPAY		58

// Train sender status
#define	STRM_TSSTAT_NONE	0
#define	STRM_TSSTAT_WDAT	1
#define STRM_TSSTAT_WACK	2

typedef	struct {
//
// Train car payload
//
	union {
		struct {
			word x : 10;
			word y : 10;
			word z : 10;
			word u : 2;
		} enc;
		lword	code;
	};
} strpw_t;

typedef	struct strblk_t strblk_t;

struct strblk_t {
//
// Queued block awaiting transmission in a car
//
	strblk_t	*next;		// We link them
	lword 		bn;		// Block number; we need it unpacked
	strpw_t		block [STRM_NCODES];
};

typedef	struct {
//
// End of train packet payload
//
	lword last;	// The last transmitted block number
	word offset;	// The backward offset to the first "askable" block
} streot_t;

#define	ack_off_s(b)	(*(b) + 1)
#define	ack_off_l(b)	((((word)(*(b)) & 0x3f) << 8) + *((b) + 1) + 1)

// ============================================================================

extern byte	RadioOn, LastRef;
extern sint	RFC;
extern cc1350_rfparams_t RFP;

void osscmn_init ();
address osscmn_xpkt (byte, byte, word);
void osscmn_xack (byte, word);
void osscmn_turn_radio (byte);
void osscmn_rfcontrol (sint, address);

// Provided by the application
void handle_rf_packet (byte, byte, const address, word);

#define toggle(a)	((a) = 1 - (a))
#define	incm(a,m)	do { if (++(a) == (m)) (a) = 0; } while (0)

#endif
