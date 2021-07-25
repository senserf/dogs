/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "tag.h"
#include "sampling.h"
#include "streaming.h"

//
// It is impossible for the queue to contain a block whose offset from the
// base (BHead->bn) is larger than MAX_OFFSET.
// Addition wins, BHead is advanced (removed) if the offset turns out large.
// We keep LastSent (last block sent); the applicable base is always determined
// by BHead->bn (and can change anytime).
//
static	lword		LastSent;
static	strblk_t	*BHead, *BTail, *CBuilt, *CCar;
static	word		NQueued, NCars, CFill;
static	aword		TSender;
static	byte		TSStat, LTrain;

static inline lword encode (address data) {
	return (((lword)(data [0] & 0xffc0)) << 22) |
	       (((lword)(data [1] & 0xffc0)) << 12) |
	       (((lword)(data [2] & 0xffc0)) <<  2) ;
}

static void delete_front () {

	strblk_t *p;

	if (BHead == NULL)
		return;

	p = BHead -> next;

	if (CCar == BHead)
		// If removing current car, reset it to next block in the
		// list
		CCar = p;

	ufree (BHead);

	if ((BHead = p) == NULL)
		BTail = NULL;

	NQueued--;
}

static void add_current () {

	CBuilt -> next = NULL;
	CBuilt -> bn = SamplesTaken;	// aka current block


	// Make sure the queue is never longer than max and the offset
	// is kosher
	while (BHead != NULL && (NQueued >= STRM_MAX_QUEUED ||
	    SamplesTaken - BHead -> bn > STRM_MAX_OFFSET))
		delete_front ();

	if (BTail == NULL)
		BHead = BTail = CBuilt;
	else
		BTail = (BTail -> next = CBuilt);

	if (CCar == NULL && TSStat == STRM_TSSTAT_WDAT) {
		// The train is running, and the dispatcher is waiting for a
		// car
		CCar = CBuilt;
		ptrigger (TSender, TSender);
	}

	NQueued++;
	SamplesTaken++;
	CBuilt = NULL;
}

static void fill_current_car (address pkt) {

	sint i;
	lword bn = CCar -> bn;

	osshdr (pkt) -> code = MESSAGE_CODE_SBLOCK;
	// The least significant byte goes into ref; this way the ref field
	// can be used directly as a modulo-256 counter of the block
	osshdr (pkt) -> ref = (byte) bn;

	bn >>= 8;
	for (i = 0; i < STRM_NCODES; i++) {
		((lword*) osspar (pkt)) [i] =
			CCar -> block [i] . code | (bn & 0x3);
		bn >>= 2;
	}
}

static void fill_eot (address pkt) {

#define	pay	((streot_t*) osspar (pkt))

	osshdr (pkt) -> code = MESSAGE_CODE_ETRAIN;
	osshdr (pkt) -> ref = LTrain;

	pay -> last = LastSent;

	pay -> offset = (BHead -> bn) > LastSent ? 0 :
		(word) LastSent - BHead -> bn + 1;
	// Note that offset == 0 means no blocks can be retransmitted (LastSent
	// is below the head), 1 means head == LastSent
#undef pay
}

fsm streaming_trainsender {

	state ST_NEWTRAIN:

		NCars = 0;
		CCar = BHead;
		TSStat = STRM_TSSTAT_NONE;
		LTrain++;

	state ST_NEXT:

		address pkt;

		if (NCars >= STRM_TRAIN_LENGTH) {
			TSStat = STRM_TSSTAT_WACK;
			sameas ST_ENDTRAIN;
		}

		if (CCar == NULL) {
			// Wait for event
			TSStat = STRM_TSSTAT_WDAT;
			when (TSender, ST_NEXT);
			release;
		}

		TSStat = STRM_TSSTAT_NONE;

		// Expedite current car; should we wait?
		pkt = tcv_wnp (ST_NEXT, RFC, STRM_NCODES * 4 + RFPFRAME +
		    sizeof (oss_hdr_t));

		fill_current_car (pkt);
		// No LBT
		tcv_endpx (pkt, NO);

		LastSent = CCar -> bn;
		CCar = CCar -> next;
		NCars++;

		delay (STRM_CAR_SPACE, ST_NEXT);
		release;

	state ST_ENDTRAIN:

		address pkt;


		if (TSStat != STRM_TSSTAT_WACK)
			// The ACK has arrived and has been processed
			sameas ST_NEWTRAIN;

		// Keep sending EOT packets waiting for an ACK
		pkt = tcv_wnp (ST_ENDTRAIN, RFC, sizeof (streot_t) + RFPFRAME +
			sizeof (oss_hdr_t));

		fill_eot (pkt);
		tcv_endpx (pkt, YES);

		delay (STRM_TRAIN_SPACE, ST_ENDTRAIN);
		when (TSender, ST_ENDTRAIN);
}

fsm streaming_generator {

	state ST_TAKE:

		word data [3];

		// We get precisely three value; accel is the only component
		read_sensor (WNONE, SENSOR_MPU9250, data);

		if (CBuilt == NULL) {
			// Next buffer
			CBuilt = (strblk_t*) umalloc (sizeof (strblk_t));
			if (CBuilt == NULL)
				// We have to be smarter than this
				sameas ST_WAIT;
			CFill = 0;
		}
		
		CBuilt -> block [CFill++] . code = encode (data);

		if (CFill == STRM_NCODES) {
			// This sets CBuilt to NULL and so on
			add_current ();
		}

	state ST_WAIT:

		delay (SampleSpace, ST_TAKE);
}

void streaming_tack (byte ref, byte *ab, word len) {
//
// Process a train ACK
//
	strblk_t *cb, *pv, *tm;
	lword	nts;
	sint	mp;

	if (TSStat != STRM_TSSTAT_WACK || ref != LTrain)
		// Just ignore
		return;

	cb = BHead;		// Current
	pv = NULL;		// Previous
	mp = 0;

	// This is the starting setting of the block number reference; this
	// cannot be a legit block to retain because it has not be indicated
	// in the ACK, so we subtract one from the minimum legit number
	nts = LastSent - STRM_MAX_OFFSET - 1;

	BTail = NULL;
	while (cb != NULL) {
		// Check if this block should stay
		// if (must_stay ()) ...
		while (cb->bn > nts) {
			// When we hit a block larger than next block from the
			// ACK, the ACK block must be skipped, because it means
			// that the requested block is not available any more
			// next_to_stay () ...
			do {
				if (mp) {
					// Doing the bit map
					if (mp == 7) {
						// Done
						mp = 0;
					} else {
try_map:
						nts++;
						if (*ab & (1 << mp++))
							// return next_to_stay
							break;
					}
					continue;
				}
				if (len == 0) {
					// No more blocks
force_end:
					// Skip all blocks to the end
					nts = MAX_LWORD;
					break;
				}
				if (*ab & 0x80) {
					// A new bit map; a zero map is a NOP
					// advancing nts by 7
					len--;
					mp = 0;
					goto try_map;
				}
				if (*ab & 0x40) {
					// Long offset
					if (len < 2) {
						// This won't happen
						len = 0;
						goto force_end;
					}
					nts += ack_off_l (ab);
					len -= 2;
					break;
				}
				nts += ack_off_s (ab);
				len--;
				break;
			} while (1);
		}

		if (cb->bn == nts) {
			// The block must stay
			BTail = cb;
			cb = (pv = cb) -> next;
		} else {
			tm = cb;
			cb = cb -> next;
			ufree (tm);
			NQueued--;
			if (pv == NULL)
				BHead = cb;
			else
				pv -> next = cb;
		}
	}

	ptrigger (TSender, TSender);
}

word streaming_start (const command_sample_t *pmt, word pml) {
//
// Assume same request format as for sampling (just the rate for now)
//
	if (Status == STATUS_SAMPLING)
		return ACK_BUSY;

	if (pml < 2)
		return ACK_LENGTH;

	if (!mpu9250_active || mpu9250_desc . components != 1)
		// The only legit component is the accel; we expect exactly
		// 3 values from the sensor
		return ACK_CONFIG;

	if (Status == STATUS_STREAMING)
		// Clear everything; we probably shouldn't be restarting
		streaming_stop ();

	if ((SamplesPerMinute = pmt->spm) == 0)
		// This is the default: one sample per second, 1 sample per
		// minute is OK
		SamplesPerMinute = 60;
	else if (SamplesPerMinute > MAX_SAMPLES_PER_MINUTE)
		SamplesPerMinute = MAX_SAMPLES_PER_MINUTE;

	// Calculate a rough estimate of the inter-sample interval in msecs;
	// we will be adjusting it to try to keep the long-term rate in samples
	// per minute as close to the target as possible

	SampleSpace = (60 * 1024) / SamplesPerMinute;
	SamplesTaken = 0;
	SampleStartSecond = seconds ();

	if (runfsm streaming_generator && runfsm sampling_corrector &&
	    (TSender = runfsm streaming_trainsender)) {
		Status = STATUS_STREAMING;
		LTrain = 0;
		return ACK_OK;
	}

	streaming_stop ();
	Status = STATUS_IDLE;
	return ACK_NORES;
}

void streaming_stop () {

	if (Status != STATUS_STREAMING)
		return;

	killall (streaming_generator);
	killall (sampling_corrector);
	killall (streaming_trainsender);

	if (CBuilt) {
		ufree (CBuilt);
		CBuilt = NULL;
	}

	while (BHead)
		delete_front ();

	NQueued = NCars = CFill = 0;
	TSStat = STRM_TSSTAT_NONE;
	LTrain = 0;
	CCar = NULL;

	Status = STATUS_IDLE;
}
