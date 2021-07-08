/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "streaming.h"

//
// It is impossible for the queue to contain a block whose offset from the
// base (BHead->bn) is larger than MAX_OFFSET.
// Addition wins, BHead is advanced (removed) if the offset turns out large.
// We keep LSent (last block sent); the applicable base is always determined
// by BHead->bn (and can change anytime).
//
static	lword		LSent;
static	strblk_t	*BHead, *BTail, *CBuilt, *CCar;
static	word		NQueued, NCars, CFill;
static	aword		TSender;
static	byte		TSStat;

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
		// We don't have to do this if the train is not running;
		// seems to do no harm, though.
		CCar = CBuilt;
		ptrigger (TSender, TSender);
	}

	NQueued++;
	SamplesTaken++;
	CBuilt = NULL;
}

static void fill_current_car (address pkt) {

	lword base;
	word off;

	// BHead determines the base block
	off = (word)(CCar -> bn - (base = BHead -> bn));
	pkt [RFPHDOFF/2] = strph_mkhd (MESSAGE_CODE_SBLOCK, off);

	for (i = 0; i < STRM_NCODES; i++) {
		((lword*) (pkt + RFPHDOFF/2 + 1)) [i] =
			CCar -> block [i] . code | (base & 0x3);
		base >>= 2;
	}
}

static void fill_eot (address pkt) {

	word off = (word) (LastSent - BHead -> bn);
	pkt [RFPHDOFF/2] = strph_mkhd (MESSAGE_CODE_ETRAIN, off);
	((streot_t*) osspar (pkt)) -> base = BHead -> bn;
}

fsm streaming_trainsender {

	word NCars;

	state ST_NEWTRAIN:

		NCars = 0;
		CCar = BHead;
		TSStat = STRM_TSSTAT_NONE;

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

		LSent = CCar -> bn;
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

		// Make sure sensor is single component!!!
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

void streaming_tack (word off, address ab, word len) {
//
// Process a train ACK
//
	



}

word streaming_start (const command_sample_t *pmt, word pml) {
//
// Assume same request format as for sampling
//
	if (Status == STATUS_SAMPLING)
		return ACK_BUSY;

	if (pml < 6)
		return ACK_LENGTH;

	if (!mpu9250_active)
		return ACK_VOID;

	if (Status == STATUS_STREAMING)
		// Clear everything; we probably shouldn't be restarting
		streaming_stop ();

	if ((SamplesPerMinute = pmt->spm) == 0)
		// This is the default: one sample per second, 1 sample per
		// minute is OK
		SamplesPerMinute = 60;
	else if (SamplesPerMinute > MAX_SAMPLES_PER_MINUTE)
		SamplesPerMinute = MAX_SAMPLES_PER_MINUTE;

	if (pmt->seconds)
		// Set the time
		SetTime = pmt->seconds - seconds ();

	// Calculate a rough estimate of the inter-sample interval in msecs;
	// we will be adjusting it to try to keep the long-term rate in samples
	// per minute as close to the target as possible

	SampleSpace = (60 * 1024) / SamplesPerMinute;
	SamplesTaken = 0;
	SampleStartSecond = seconds ();

	if (runfsm streaming_generator && runfsm sampling_corrector &&
	    (TSender = runfsm streaming_trainsender)) {
		Status = STATUS_STREAMING;
		return ACK_OK;
	}

	streaming_stop ();
	Status = STATUS_IDLE;
	return ACK_NORES;
}
