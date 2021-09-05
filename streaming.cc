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
static	lword		LastSent, LastGenerated;
static	strblk_t	*BHead, *BTail, *CBuilt, *CCar;
static	word		NQueued, NCars, CFill;
static	aword		TSender;
static	byte		TSStat, LTrain, TFlags;

// Can be used to normalize the values, e.g., for compression
#define	ACCBIAS		0x0

static inline lword encode (address data) {
	return (((lword)((data [0] + ACCBIAS) & 0xffc0)) << 16) |
	       (((lword)((data [1] + ACCBIAS) & 0xffc0)) <<  6) |
	       (((lword)((data [2] + ACCBIAS) & 0xffc0)) >>  4) ;
}

static void delete_front () {

	strblk_t *p;

	// Never called with BHead == NULL
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

	// Block numbering starts from 1, Last Received can be initialized to
	// zero
	CBuilt -> next = NULL;
	CBuilt -> bn = ++LastGenerated;	// aka current block

	// Make sure the queue is never longer than max and the offset
	// is kosher
	while (NQueued >= TagParams.max_queued ||
	  (BHead != NULL && (LastGenerated - BHead -> bn >=
	    STRM_MAX_BLOCKSPAN))) {
		delete_front ();
		StreamStats . queue_drops ++;
		TFlags |= STRM_TFLAG_QDR;
	}

	if (BTail == NULL)
		BHead = BTail = CBuilt;
	else
		BTail = (BTail -> next = CBuilt);

	if (CCar == NULL) {
		CCar = CBuilt;
		if (TSStat == STRM_TSSTAT_WDAT)
			// The dispatcher is waiting for a car
			ptrigger (TSender, TSender);
	}

	NQueued++;
	CBuilt = NULL;
}

static void fill_current_car (address pkt) {

	sint i;
	lword bn = CCar -> bn;

	pkt_osshdr (pkt) -> code = MESSAGE_CODE_SBLOCK;
	// The least significant byte goes into ref; this way the ref field
	// can be used directly as a modulo-256 counter of the block
	pkt_osshdr (pkt) -> ref = (byte) bn;

	bn >>= 8;
	for (i = 0; i < STRM_NCODES; i++) {
		((lword*) pkt_payload (pkt)) [i] =
			CCar -> block [i] | (bn & 0x3);
		bn >>= 2;
	}
}

static void fill_eot (address pkt) {

#define	pay	((message_etrain_t*) pkt_payload (pkt))

	pkt_osshdr (pkt) -> code = MESSAGE_CODE_ETRAIN;
	pkt_osshdr (pkt) -> ref = LTrain;

	pay -> last = LastSent;
	pay -> offset = (BHead == NULL || (BHead -> bn) > LastSent) ? 0 :
		(word) (LastSent - BHead -> bn + 1);
	// Note that offset == 0 means no blocks can be retransmitted (LastSent
	// is below the head), 1 means head == LastSent
	pay -> voltage = VOLTAGE;
	pay -> flags = TFlags;
#undef pay
}

fsm streaming_trainsender {

	word train_space;

	state ST_NEWTRAIN:

		NCars = 0;
		CCar = BHead;
		TSStat = STRM_TSSTAT_NONE;
		LTrain++;
		TFlags = 0;

	state ST_NEXT:

		address pkt;

		if (NCars >= TagParams.train_length) {
			TSStat = STRM_TSSTAT_WACK;
			train_space = TagParams.min_train_space;
			sameas ST_ENDTRAIN;
		}

		if (CCar == NULL) {
			// Wait for event
			TSStat = STRM_TSSTAT_WDAT;
			when (TSender, ST_NEXT);
			release;
		}

		TSStat = STRM_TSSTAT_NONE;

		if ((pkt = tcv_wnp (ST_NEXT, RFC, STRM_NCODES * 4 +
		    PKT_FRAME_ALL)) != NULL) {

			fill_current_car (pkt);
			// No LBT
			tcv_endpx (pkt, NO);
		}

		LastSent = CCar -> bn;
		CCar = CCar -> next;
		NCars++;

		delay (TagParams.car_space, ST_NEXT);
		release;

	state ST_ENDTRAIN:

		address pkt;

		if (TSStat != STRM_TSSTAT_WACK)
			// The ACK has arrived and has been processed
			sameas ST_NEWTRAIN;

		// Keep sending EOT packets waiting for an ACK
		if ((pkt = tcv_wnp (ST_ENDTRAIN, RFC,
			sizeof (message_etrain_t) + PKT_FRAME_ALL)) != NULL) {

			fill_eot (pkt);
			tcv_endpx (pkt, NO);
		}

		delay (train_space, ST_ENDTRAIN);
		when (TSender, ST_ENDTRAIN);
		if (train_space < STRM_MAX_TRAIN_SPACE)
			train_space++;
}

#if MPU9250_FIFO_BUFFER_SIZE

// Use FIFO (this doesn't work with LP mode)
static word sg_delay = 4;

static void fifo_start () {

	lword d;

	// Calculate a safe delay
	d = (1024 * 60 * (MPU9250_FIFO_BUFFER_SIZE/2)) / mpu9250_desc.rate;

	sg_delay = d > 1024 ? 1024 : (word) d;

	// diag ("SD: %u", sg_delay);

	mpu9250_fifo_start ();
}

#define	fifo_stop()	mpu9250_fifo_stop ()

fsm streaming_generator {

	word data [3 * MPU9250_FIFO_BUFFER_SIZE];

	state ST_TAKE:

		word nw, *dt;

		if ((nw = mpu9250_fifo_get (data, MPU9250_FIFO_BUFFER_SIZE)) ==
		    0) {
			delay (sg_delay, ST_TAKE);
			release;
		}

		if (nw == MPU9250_FIFO_OVERFLOW) {
			StreamStats . fifo_overflows ++;
			TFlags |= STRM_TFLAG_FOV;
			delay (1, ST_TAKE);
			release;
		}

		dt = data;

		while (nw >= 3) {

			if (CBuilt == NULL) {
				// Next buffer
				CBuilt =
				    (strblk_t*) umalloc (sizeof (strblk_t));
				if (CBuilt == NULL) {
					// We have to be smarter than this
					StreamStats . malloc_failures ++;
					TFlags |= STRM_TFLAG_MAL;
					sameas ST_TAKE;
				}
				CFill = 0;
			}
		
			CBuilt -> block [CFill++] = encode (dt);
			SamplesTaken++;

			if (CFill == STRM_NCODES) {
				// This sets CBuilt to NULL
				add_current ();
			}

			dt += 3;
			nw -= 3;
		}

	sameas ST_TAKE;
}

#else

// No FIFO

#define	fifo_start()	CNOP
#define	fifo_stop()	CNOP

fsm streaming_generator {

	state ST_TAKE:

		word data [3];

		read_mpu9250 (WNONE, data);

		if (CBuilt == NULL) {
			// Next buffer
			CBuilt = (strblk_t*) umalloc (sizeof (strblk_t));
			if (CBuilt == NULL) {
				// We have to be smarter than this
				StreamStats . malloc_failures ++;
				TFlags |= STRM_TFLAG_MAL;
				sameas ST_WAIT;
			}
			CFill = 0;
		}
		
		CBuilt -> block [CFill++] = encode (data);
		SamplesTaken++;

		if (CFill == STRM_NCODES) {
			// This sets CBuilt to NULL and so on
			add_current ();
		}

	initial state ST_WAIT:

		ready_mpu9250 (ST_TAKE);
}

#endif	/* FIFO or no FIFO */

void streaming_tack (byte ref, byte *ab, word plen) {
//
// Process a train ACK
//
	strblk_t *cb, *pv, *tm, *ta;
	lword	nts, boundary;
	sint	mp;
	word 	rlen;

// ============================================================================
// Initialize for scanning through the block queue
#define ini_cb	do { cb = BHead; pv = NULL; ta = NULL; } while (0)
// Advance the queue pointer
#define	adv_cb	do { ta = cb; cb = (pv = cb)->next; } while (0)
// Delete current item
#define	del_cb	do { tm = cb; cb = cb->next; ufree (tm); NQueued--; \
		    if (pv == NULL) BHead = cb; else pv->next = cb; } while (0)
// Complete packet queue processing
#define	end_cb	do { if (cb == NULL) BTail = ta; } while (0)
// ============================================================================

	if (TSStat != STRM_TSSTAT_WACK || ref != LTrain) {
		// Just ignore
		return;
	}

	mp = 0;
	rlen = plen;		// Remaining length

	// This is the starting setting of the block number reference; this
	// cannot be a block to retain because it has not been indicated
	// in the ACK, so it is one less than the minimum legit number than
	// the ACK can specify. Note that when we start, there is no history,
	// so the first block is numbered 1 (not zero).
	nts = (LastSent < STRM_MAX_BLOCKSPAN) ? 0 :
		LastSent - STRM_MAX_BLOCKSPAN;

	ini_cb;

	while (cb != NULL && cb->bn <= LastSent) {
		// Check if the block should stay in the queue for
		// retransmission. Block > LastSent stay uncoditionally because
		// the are new and not covered by the ACK. We keep scanning
		// through the ACK until we find a block that is >= to the
		// current block.
		while (nts < cb->bn) {
			do {
				if (mp) {
					// Doing the bit map
					if (mp == 7) {
						// Done
						mp = 0;
						rlen--;
						ab++;
					} else {
try_map:
						nts++;
						if (*ab & (1 << mp++))
							break;
					}
					continue;
				}

				if (rlen == 0)
					// No more blocks in the ACK
					goto end_ack;

				if (*ab & 0x80) {
					// A new bit map; a zero map is a NOP
					// advancing nts by 7
					mp = 0;
					goto try_map;
				}

				if (*ab & 0x40) {
					// Long offset
					if (rlen < 2) {
						// This won't happen
						rlen = 0;
						goto end_ack;
					}
					nts += (((word)(*ab) & 0x3f) << 8);
					rlen--;
					ab++;
				}

				// A short offset or the second part of a
				// long one
				nts += *ab + 1;
				rlen--;
				ab++;
				break;

			} while (1);
		}

		// nts >= cb->bn

		if (cb->bn == nts) {
			// The block must stay because the ACK asks for it
			adv_cb;
		} else {
			// The block can be dropped: nts > bn
			del_cb;
		}
	}

end_ack:

	// Queue boundary
	if (plen < STRM_MAX_ACKPAY)
		nts = LastSent;

	while (cb != NULL && cb->bn <= LastSent)
		// Delete those less than or equal to:
		// LastSent - when the ACK was complete
		// nts      - when the ACK appears overflown
		// The second case covers a bit map as the last
		// item after which nts may extend beyond the
		// last requested block (implicitely acknowledging
		// some)
		del_cb;

	end_cb;
		
	TSStat = STRM_TSSTAT_NONE;
	ptrigger (TSender, TSender);
#undef	ini_cb
#undef	adv_cb
#undef	del_cb
#undef	end_cb
}

word streaming_start (const command_stream_t *par, word pml) {
//
// Assume same request format as for sampling (just the rate for now)
//
	word ret;
	const byte *buf;

	if (Status == STATUS_SAMPLING)
		return ACK_BUSY;

	if (pml < 2)
		return ACK_PARAM;

	if (par->confdata.size != 0) {
		// Full automatic setup
		if ((ret = sensing_configure (&(par->confdata), pml)) != ACK_OK)
			return ret;
		// All sensors off
		sensing_all_off ();
		// Turn on the IMU
		sensing_turn (0x81);
	}

	if (!mpu9250_active || mpu9250_desc . components != 1 ||
	    mpu9250_desc . evtype != 2)
		// The only legit component is the accel; we expect exactly
		// 3 values from the sensor
		return ACK_CONFIG;

	if (Status == STATUS_STREAMING)
		// Clear everything; we probably shouldn't be restarting
		streaming_stop ();

	LastGenerated = SamplesTaken = 0;
	SamplesPerMinute = mpu9250_desc.rate;

	fifo_start ();
	if ((TSender = runfsm streaming_trainsender) &
	    runfsm streaming_generator) {
		Status = STATUS_STREAMING;
		SamplesTaken = 0;
		LTrain = 0;
		bzero (&StreamStats, sizeof (StreamStats));
		powerup ();
		return ACK_OK;
	}

	streaming_stop ();
	fifo_stop ();
	return ACK_NORES;
}

void streaming_stop () {

	if (Status != STATUS_STREAMING)
		return;

	killall (streaming_generator);
	killall (streaming_trainsender);
	fifo_stop ();

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
	powerdown ();
}
