/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "pegstream.h"

static byte bmap [STRM_MAP_SIZE];
static lword lsent, bbase, borig, lrcvd;

static byte ackb [STRM_MAX_ACKPAY];
static sint abeg, aend, acnt, aibm;
static lword alst;

#define	bme(bo)	bmap [ (((bo) - bbase + borig) & STRM_MAP_MASK) ]
#define	incr_aend() do { if (++aend == STRM_MAX_ACKPAY) aend = 0; } while (0)

static void trim_bitmap (lword bo) {
//
// Adjust the bitmap to accommodate the most recent block number (/8)
//
	lword bh, ne;

	if (bo - bbase >= STRM_MAP_SIZE) {
		// The new block number doesn't fit; ne = the number of entries
		// to remove from the map (skip)
		if ((ne = bo - bbase - STRM_MAP_SIZE + 1) >= STRM_MAP_SIZE) {
			// Bypassing the entire map
			borig = 0;
			bzero (bmap, STRM_MAP_SIZE);
			bbase = bo;
			return;
		}
		if ((bh = borig + ne) >= STRM_MAP_SIZE) {
			// Wrapping around
			bh &= STRM_MAP_MASK;
			bzero (bmap + borig, STRM_MAP_SIZE - borig);
			bzero (bmap, bh);
		} else {
			// A single chunk
			bzero (bmap + borig, ne);
		}
		borig = bh;
		bbase += ne;
	}
}

static inline void init_ack () {

	abeg = aend = acnt = 0;
	alst = lsent - STRM_MAX_OFFSET - 1;
	aibm = -1;
}

static inline void close_ack () {

	if (aibm >= 0) {
		// Close the open map
		incr_aend ();
		aibm = -1;
	}
}

static void incr_ack () {
//
// Makes sure there is at least one free byte in the ACK
//
	sint sabg, sbbg, skip;
	word r, b, i;
	byte CB;

#define	incr_abeg()	do { sabg = sbbg; sbbg = abeg; CB = ackb [abeg]; \
			     if (++abeg == STRM_MAX_ACKPAY) abeg = 0; \
			     skip++; } while (0)

	if (acnt < STRM_MAX_ACKPAY)
		// Statistically, this is what will happen
		return;

	skip = 0;
	r = 0;

	// Skip the first byte which we must skip
	incr_abeg ();

	if (CB & 0x80) {
		// Starts with a bitmap: delete, update offset
		r += 7;
		// Note that this offset, as such, is wrong, because the
		// block at it need not be deleted, but it will work as an
		// offset to any subsequent offset; so we just accumulate
		// the offset at this stage; whatever will be replaced is
		// the NEXT item
	} else {
		if (CB & 0x40) {
			// A long offset
			r += ((word)(CB & 0x3f)) << 8;
			// Get hold of the next one
			incr_abeg ();
		}
		// Also covers a short offset
		r += CB;
	}

	// After this, skip == 1 or skip == 2; we still need more, even when
	// skip == 2, it just means that we have skipped a long offset

	while (1) {

		// We may need to repeat this

		incr_abeg ();

		if (CB & 0x80) {
			// A bit map: find the max bit set
			for (i = b = 0; i < 7; i++)
				if (ackb [abeg] & (1 << i))
					b = i;
			if (r + b <= 63) {
				// A short offset will do (to the last block
				// covered by the map)
				r += b;
SOff:
				ackb [abeg = sbbg] = r;
				acnt -= skip - 1;
				return;
			} else if (skip > 2) {
				// A long offset can be used
				r += b;
LOff:
				ackb [abeg = sabg] = 0x40 | ((r >> 8) & 0x3f);
				ackb [sbbg] = (byte) r;
				acnt -= skip - 2;
				return;
			}
			// Must keep going: remove the bitmap, and look for
			// next item
			r += 7;
		} else {
			if (CB & 0x40) {
				// A long offset
				r += ((word)(CB & 0x3f)) << 8;
				incr_abeg ();
			}
			// Covers the short offset case
			r += CB;
			if (r <= 63)
				goto SOff;
			if (skip > 2)
				goto LOff;
		}
	}
#undef incr_abeg
}

static void add_ack (lword bn) {
//
// Adds a block number to the ACK
//
	lword d;

	if (bn <= alst || bn > lsent)
		return;

	// aend shows where to write next

	if (aibm >= 0) {
		// Within the bit map, aibm is the last-set bit, alst is the
		// last set bn, aend points to the map
		if (bn - alst < 7 - aibm) {
			// Add to the bit map
			aibm += (bn - alst);
			ackb [aend] |= (1 << aibm);
			alst = bn;
			return;
		} else {
			// Close the map
			alst += 6 - aibm;
			aibm = -1;
			incr_aend ();
		}
	}

	// Need room; call this when you are about to write something into a
	// new byte
	incr_ack ();

	if ((d = bn - alst - 1) <= 5) {
		// Use a bit map, if it stands a chance to be better than
		// offset
		aibm = d;
		ackb [aend] = 0x80 | (1 << aibm);
		alst = bn;
		acnt++;
		return;
	}

	if (d > 63) {
		// Need a long offset
		incr_ack ();
		ackb [aend] = 0x40 | ((d >> 8) & 0x3f);
		acnt++;
		incr_aend ();
	}

	ackb [aend] = (byte) d;
	acnt++;
	incr_aend ();
	alst = bn;
}
		
void pegstream_init () {

	bzero (bmap, STRM_MAP_SIZE);
	borig = 0;
	bbase = lsent = 0;
	lrcvd = lsent - 1;
}

void pegstream_tally_block (byte ref, address pkt) {

	// Extract the block number, this is the only thing we care about
	lword bn, bo;
	sint bb;

	for (bn = ref, bb = 0; bb < STRM_NCODES; bb++)
		bn |= (((lword*)pkt) [bb] & 0x3) << ((bb + bb) + 8);

	// Update the map

	if (bn < lrcvd) {
		// This is a block from the past, remove it from the map
		bo = bn >> 3;
		if ((bo = bn >> 3) < bbase)
			// Too old
			return;
		bb = bn & 0x7;
		bme (bo) &= ~(1 << bb);
		return;
	}

	if (++lrcvd == bn)
		// This is what we are betting on, nothing to do
		return;

	// We have a hiccup, nm = the number of missing blocks
	bo = lrcvd >> 3;
	bb = lrcvd & 0x7;
	trim_bitmap (bo);

	do {
		bme (bo) |= (1 << bb);
		if (++lrcvd == bn)
			break;
		if (bb == 7) {
			bo++;
			bb = 0;
			trim_bitmap (bo);
		} else {
			bb++;
		}
	} while (1);
}

void pegstream_eot (byte ref, address pkt) {
//
// Received EOT
//
	lword bo, bn;
	address msg;
	sint bb, nm;
	word as, of;

	// Last sent
	lsent = ((streot_t*) pkt) -> last;
	// Back offset to the earliest available block
	of = ((streot_t*) pkt) -> offset;

	// A sanity check
	if (lsent < lrcvd || of > lsent)
		return;

	if (lsent > lrcvd) {
		// Add the missing blocks at the tail
		nm = (sint) (lsent - lrcvd);
		bo = lrcvd + 1;
		bb = bo & 0x7;
		bo >>= 3;
		trim_bitmap (bo);
		do {
			bme (bo) |= (1 << bb);
			if (--nm == 0)
				break;
			if (bb == 7) {
				bo++;
				bb = 0;
				trim_bitmap (bo);
			} else {
				bb++;
			}
		} while (1);
	}

	// Build the ACK packet
	init_ack ();

	bn = bbase;
	bo = borig;

	while (bn <= lsent) {

		if (bmap [bo]) {
			for (bb = 0; bb < 8; bb++)
				if (bmap [bo] & (1 << bb))
					add_ack (bn + bb);
		}

		bn += 8;
		if (++bo == STRM_MAP_SIZE)
			bo = 0;
	}

	// Copy the ACK to the packet
	close_ack ();

	if ((as = acnt) & 1)
		// Make sure the packet payload length is even
		as++;
	if ((msg = osscmn_xpkt (MESSAGE_CODE_STRACK, ref, as)) != NULL) {
		if (abeg >= aend) {
			// A single chunk
			memcpy (osspar (msg), ackb + abeg, acnt);
		} else {
			// Two chunks
			memcpy (osspar (msg), ackb + abeg, STRM_MAX_ACKPAY -
				abeg);
			memcpy (osspar (msg), ackb, aend);
		}
	}

	if (acnt & 1)
		// This is a void dummy map
		((byte*)(osspar (msg))) [acnt] = 0x80;

	tcv_endpx (msg, NO);
}
