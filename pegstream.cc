/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "pegstream.h"

static byte bmap [STRM_MAP_SIZE];
static lword lsent, bbase, lrcvd;

static byte ackb [STRM_MAX_ACKPAY];
static sint aend, aibm;
static lword alst;

// Bit count per byte
static const byte bit_count [256] =
	{ 	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
		4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8	 };

// The bitmap holds up to STRM_MAX_BLOCKSPAN block status bits counting
// from bbase; bo is assumed to be divided by 8 (representing an aligned
// 8-tuple)
#define	bme(bo)	bmap [(bo) & STRM_MAP_MASK]

static void m_bzero (byte *m, word cnt) {
//
// This is a version of bzero counting ones in the cleared area; this tells
// us how many blocks have been irretrievably lost (from our perspective)
//
	while (cnt--) {
		if (*m) {
			loss_count += bit_count [*m];
			*m = 0;
		}
		m++;
	}
}

static inline void shift_bitmap (lword by_this) {

	word pi, ci;

	// Previous base index
	pi = bbase & STRM_MAP_MASK;
	ci = (bbase += by_this) & STRM_MAP_MASK;

	// Need to zero out the reclaimed area between pi and ci - 1
	if (pi < ci) {
		m_bzero (bmap + pi, ci - pi);
	} else {
		m_bzero (bmap + pi, STRM_MAP_SIZE - pi);
		m_bzero (bmap, ci);
	}
}

static void extend_bitmap (lword bo) {
//
// Make sure the bitmap accommodates the block number bo; bo is already
// aligned (i.e., divided by 8)
//
	lword ne;

	if (bo - bbase >= STRM_MAP_SIZE) {
		// The base must be shifted, ne == by how much
		ne = bo - bbase - STRM_MAP_SIZE + 1;
		if (ne >= STRM_MAP_SIZE) {
			// By more than the map length, so we invalidate the
			// entire content
			m_bzero (bmap, STRM_MAP_SIZE);
			// Can start from here
			bbase = bo;
		} else {
			shift_bitmap (ne);
		}
	}
}

static inline void shrink_bitmap (lword bo) {
//
// Adjust the bitmap to remove the useless head after reception of EOT; bo
// is the earliest block that still can be asked for
//
	lword ne;

	if (bo > bbase) {
		ne = bo - bbase;
		if (ne >= STRM_MAP_SIZE) {
			m_bzero (bmap, STRM_MAP_SIZE);
			bbase = bo;
		} else {
			shift_bitmap (ne);
		}
	}
}

static inline void add_to_map (lword from, lword upto) {
//
// Add blocks from through upto (inclusively) to the map as missing
//
	lword bo;
	sint bb;

	if ((bo = from >> 3) < bbase)
		// This is impossible
		return;

	bb = from & 0x7;

	// Make sure bo can be accommodated
	extend_bitmap (bo);

	do {
		bme (bo) |= (1 << bb);
		if (from == upto)
			return;
		from++;
		if (bb == 7) {
			bo++;
			bb = 0;
			extend_bitmap (bo);
		} else {
			bb++;
		}
	} while (1);
}

static inline void remove_from_map (lword bn) {
//
// Removes one block from the map
//
	lword bo;

	if (((bo = bn >> 3) >= bbase) && (bo - bbase < STRM_MAP_SIZE))
		// Covered by the map
		bme (bo) &= ~(1 << (bn & 0x7));
}

static inline void init_ack (word offset) {

	aend = 0;
	if (lsent >= offset)
		shrink_bitmap ((lsent - offset + 1) >> 3);
	// Initial reference for offsets
	alst = (lsent < STRM_MAX_BLOCKSPAN) ? 0 : lsent - STRM_MAX_BLOCKSPAN;
	aibm = -1;
}

static inline void close_ack () {

	if (aibm >= 0) {
		// Close the open map
		aend++;
		aibm = -1;
	}
}

static inline Boolean add_ack (lword bn) {
//
// Adds a block number to the ACK
//
	lword d;

	if (bn <= alst || bn > lsent)
		// A sanity check: must be increasing and up to lsent (L)
		return NO;

	if (aibm >= 0) {
		// A bitmap is open, aibm points to the next bit, aend stays
		// put at the bitmap byte, alst is the last set bn
		if (bn - alst < 7 - aibm) {
			// Falls within the current map
			aibm += (bn - alst);
			ackb [aend] |= (1 << aibm);
			alst = bn;
			return NO;
		} else {
			// Close the map
			alst += 6 - aibm;
			aibm = -1;
			aend++;
		}
	}

	if (aend == STRM_MAX_ACKPAY)
		// Packet full
		return YES;

	if ((d = bn - alst - 1) <= 5) {
		// A bit map costs the same memory as a short offset, but
		// offsets are easier to handle. Use bit map, if there's a
		// chance (at this point) that it will cover more than one
		// block
		aibm = (sint) d;
		ackb [aend] = 0x80 | (1 << aibm);
		alst = bn;
		return NO;
	}

	if (d > 63) {
		// Need a long offset
		if (aend == STRM_MAX_ACKPAY - 1)
			// No room
			return YES;
		ackb [aend++] = 0x40 | ((d >> 8) & 0x3f);
		ackb [aend++] = (byte) d;
	} else {
		ackb [aend++] = (byte) d;
	}

	alst = bn;
	return NO;
}

void pegstream_init () {

	bzero (bmap, STRM_MAP_SIZE);
	lrcvd = bbase = lsent = 0;
	// This counts blocks that the Peg perceives as irretrievably lost
	// which means that they were shifted out of the map
	loss_count = 0;
}

void pegstream_tally_block (byte ref, address pkt) {
//
// Update the bit map upon block reception
//
	lword bn, bo;
	sint bb;

	// Decode the block number
	for (bn = ref, bb = 0; bb < STRM_NCODES; bb++)
		bn |= (((lword*)pkt) [bb] & 0x3) << ((bb + bb) + 8);

	if (bn <= lrcvd) {
		// This is a block from the past, remove it from the map. Note
		// that lrcvd separates blocks from the past (if they were
		// missing, they are covered by the map) from those yet to
		// arrive.
		remove_from_map (bn);
		return;
	}

	// Here we have bn > lrcvd
	if (++lrcvd == bn)
		// This is what we are betting on, nothing to do
		return;

	// Add all blocks in between as missing (delayed); we will have to ask
	// for them in the ACK
	add_to_map (lrcvd, bn - 1);

	lrcvd = bn;
}

void pegstream_eot (byte ref, address pkt) {
//
// Received EOT
//
	lword bo, bn, bc;
	address msg;
	sint bb;
	word of;

	// Last sent
	lsent = ((message_etrain_t*) pkt) -> last;
	// Back offset to the earliest available block
	of = ((message_etrain_t*) pkt) -> offset;

	// A sanity check
	if (lsent < lrcvd || of > lsent)
		return;

	if (lsent > lrcvd) {
		// This doesn't agree with our idea of the last block which
		// means that the tail has been lost
		add_to_map (lrcvd + 1, lsent);
		lrcvd = lsent;
	}

	// Trim off the bit map and start the ACK
	init_ack (of);

	// Add all missing blocks <= lsent to the ACK
	for (bn = bbase, bo = lsent >> 3; bn <= bo; bn++) {
		// bn indexes 8-tuples, quickly skip zero entries
		if (bmap [of = bn & STRM_MAP_MASK] != 0) {
			bc = bn << 3;
			for (bb = 0; bb < 8; bb++)
				if (bmap [of] & (1 << bb))
					if (add_ack (bc + bb))
						break;
		}
	}

	close_ack ();

	// Copy the ACK to the packet; make sure tha packet length is even
	if ((msg = osscmn_xpkt (MESSAGE_CODE_STRACK, ref, aend)) != NULL) {
		memcpy (pkt_payload (msg), ackb, aend);
		if (aend & 1)
			// Add a dummy NOP map
			((byte*)(pkt_payload (msg))) [aend] = 0x80;
		tcv_endpx (msg, NO);
	}
}
