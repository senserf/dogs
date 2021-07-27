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
static sint abeg, aend, acnt, aibm;
static lword alst;

// The bitmap holds at least STRM_MAX_OFFSET block status bits counting
// from bbase; bo is assumed to be divided by 8 (representing an aligned
// 8-tuple)
#define	bme(bo)	bmap [(bo) & STRM_MAP_MASK]

static void trim_bitmap (lword bo) {
//
// Adjust the bitmap to accommodate the most recent block number (/8)
//
	lword ne;
	word pi, ci;

	// Both bo and bbase are 8-tuple indexes
	if (bo - bbase >= STRM_MAP_SIZE) {
		// The base must be shifted, ne == by how much
		ne = bo - bbase - STRM_MAP_SIZE + 1;
		if (ne >= STRM_MAP_SIZE) {
			// By more than the map length, so we invalidate the
			// entire content
			bzero (bmap, STRM_MAP_SIZE);
			// Can start straight from here
			bbase = bo;
			return;
		}

		// Previous base index
		pi = bbase & STRM_MAP_MASK;
		ci = (bbase += ne) & STRM_MAP_MASK;

		// Need to zero out the reclaimed area between pi and ci - 1
		if (pi < ci) {
			bzero (bmap + pi, ci - pi);
		} else {
			bzero (bmap + pi, STRM_MAP_SIZE - pi);
			bzero (bmap, ci);
		}
	}
}

static inline void add_missing (lword from, lword upto) {
//
// Add blocks from to upto (inclusively) to the map as missing
//
	lword bo;
	sint bb;

	bo = from >> 3;
	bb = from & 0x7;
	trim_bitmap (bo);

	do {
		bme (bo) |= (1 << bb);
		if (from == upto)
			break;
		from++;
		if (bb == 7) {
			bo++;
			bb = 0;
			trim_bitmap (bo);
		} else {
			bb++;
		}
	} while (1);
}

static inline void init_ack () {

	abeg = aend = acnt = 0;
	alst = lsent - STRM_MAX_OFFSET - 1;
	aibm = -1;
}

static inline void close_ack () {

	if (aibm >= 0) {
		// Close the open map
		incm (aend, STRM_MAX_ACKPAY);
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
//
// Tried to have it as a nested function, doesn't work in C++, pity:
//	-- strore tha last two skipped indexes in sabg/sbbg
//	-- set CB to current entry
//	-- skip current entry
//	-- keep track how many skipped since start
//
#define	incr_abeg()	do { sabg = sbbg; CB = ackb [sbbg = abeg]; \
			     incm (abeg, STRM_MAX_ACKPAY); skip++; } while (0)

	if (acnt < STRM_MAX_ACKPAY)
		// Statistically, this is what will happen, we are within
		// limits
		return;

	// r is the running base (for offsets)
	r = 0;

	// Skip the first byte which we must skip, no matter what; the first
	// case of incr_abeg is a bit simpler than the rest
	// incr_abeg ();
	CB = ackb [sbbg = abeg];
	incm (abeg, STRM_MAX_ACKPAY);
	skip = 1;

	if (CB & 0x80) {
		// Starts with a bitmap: delete, update offset
		r += 7;
		// Note that this offset, as such, is wrong, because the
		// block at it need not be deleted, but it will work as an
		// offset to any subsequent offset; so we just accumulate
		// the offset at this stage; whatever will be actually replaced
		// (inserted) is the NEXT item
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
	// skip == 2, it just means that we have skipped a long offset which
	// may have only gotten longer

	while (1) {

		// Not really a loop, but may have to repeat

		incr_abeg ();

		if (CB & 0x80) {
			// A bit map: find the max bit set; at least one must
			// be at this stage; the only possible case of a NOP
			// is the last (final) even byte of the packet
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
				// We can afford a long offset
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
		// Must be increasing and up to lsent (L)
		return;

	if (aibm >= 0) {
		// A bitmap is open, aibm points to the next bit, aend stays
		// put at the bitmap byte, alst is the last set bn
		if (bn - alst < 7 - aibm) {
			// Falls within the current map
			aibm += (bn - alst);
			ackb [aend] |= (1 << aibm);
			alst = bn;
			return;
		} else {
			// Close the map
			alst += 6 - aibm;
			aibm = -1;
			incm (aend, STRM_MAX_ACKPAY);
		}
	}

	// Make sure got at least one spare byte
	incr_ack ();

	if ((d = bn - alst - 1) <= 5) {
		// A bit map costs the same mempry as a short offset, but
		// offsets are easier to handle. Use bit map, if there's a
		// chance (at this point) that it will cover more than one
		// block
		aibm = (sint) d;
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
		incm (aend, STRM_MAX_ACKPAY);
	}

	ackb [aend] = (byte) d;
	acnt++;
	incm (aend, STRM_MAX_ACKPAY);
	alst = bn;
}
		
void pegstream_init () {

	bzero (bmap, STRM_MAP_SIZE);
	bbase = lsent = 0;
	lrcvd = lsent - 1;
}

void pegstream_tally_block (byte ref, address pkt) {
//
// Update the bit map
//
	lword bn, bo;
	sint bb;

	for (bn = ref, bb = 0; bb < STRM_NCODES; bb++)
		bn |= (((lword*)pkt) [bb] & 0x3) << ((bb + bb) + 8);

	if (bn < lrcvd) {
		// This is a block from the past, remove it from the map
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

	add_missing (lrcvd, bn - 1);
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
	lsent = ((streot_t*) pkt) -> last;
	// Back offset to the earliest available block
	of = ((streot_t*) pkt) -> offset;

	// A sanity check
	if (lsent < lrcvd || of > lsent)
		return;

	if (lsent > lrcvd)
		add_missing (lrcvd + 1, lsent);

	// Build the ACK packet
	init_ack ();

	for (bn = bbase, bo = lsent >> 3; bn <= bo; bn++) {
		// bn indexes 8-tuples, quickly skip zero entries
		if (bmap [of = bn & STRM_MAP_MASK] != 0) {
			bc = bn << 3;
			for (bb = 0; bb < 8; bb++)
				if (bmap [of] & (1 << bb))
					add_ack (bc + bb);
		}
	}

	close_ack ();

	// Copy the ACK to the packet; make sure tha packet length is even
	if ((msg = osscmn_xpkt (MESSAGE_CODE_STRACK, ref, (acnt+1) & ~1)) !=
	    NULL) {
		if (abeg <= aend) {
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
		// Add a NOP zero map
		((byte*)(osspar (msg))) [acnt] = 0x80;

	tcv_endpx (msg, NO);
}
