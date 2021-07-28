/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/

#include "tag.h"
#include "ossint.h"
#include "sensing.h"
#include "sampling.h"

word	SamplesPerMinute,	// Target rate
	SampleSpace;		// Adjustable space to meet target rate

lword	SampleStartSecond,	// When sampling started
	SamplesTaken;		// Samples taken so far

// ============================================================================

fsm sampling_generator {
//
// These sensors are sampled in the background: HDC1000, OPT3001, BMP280
//
	state SM_TAKE:

		address msg;
		message_report_t *pmt;
		word bl;

		// Calculate the report size
		bl = sensing_report (NULL, NULL);

		if ((msg = osscmn_xpkt (message_report_code, LastRef,
			sizeof (message_report_t) + bl)) == NULL) {
			// Failure, do we skip?
			if (SampleSpace > 128) {
				// Some heuristics
				delay (16, SM_TAKE);
				release;
			}
			// Just skip
			sameas SM_DELAY;
		}

		// Fill in the message
		pmt = (message_report_t*) pkt_payload (msg);
		pmt->sample = (word) SamplesTaken;
		pmt->data.size = sensing_report ((byte*)(pmt->data.content),
			&(pmt->layout));

		tcv_endpx (msg, YES);
		SamplesTaken++;
		
	initial state SM_DELAY:

		delay (SampleSpace, SM_TAKE);
}

fsm sampling_corrector {

	lword NextMinuteBoundary;

	state STC_START:

		NextMinuteBoundary = seconds () + 60;

	state STC_WAIT_A_MINUTE:

		lword s;

		// Wait until hit the next minute boundary
		if ((s = seconds ()) < NextMinuteBoundary) {
			delay ((word)((NextMinuteBoundary - s) * 1023),
				STC_WAIT_A_MINUTE);
			release;
		}

		s = (SampleSpace *
			(((NextMinuteBoundary - SampleStartSecond) / 60) *
				SamplesPerMinute)) / SamplesTaken;

		SampleSpace = s > MAX_SAMPLE_SPACE ? MAX_SAMPLE_SPACE :
			(s < MIN_SAMPLE_SPACE ? MIN_SAMPLE_SPACE : (word) s);

		NextMinuteBoundary += 60;

		delay (999, STC_WAIT_A_MINUTE);
}

// ============================================================================

void sampling_stop () {

	killall (sampling_generator);
	killall (sampling_corrector);
	SamplesTaken = SampleStartSecond = 0;
	SamplesPerMinute = 0;
	Status = STATUS_IDLE;
}

word sampling_start (const command_sample_t *pmt, word pml) {
//
// Request layout:
//	word	spm;		[samples per minute]
//
	if (Status == STATUS_STREAMING)
		// We should be tacitly ignoring it
		return ACK_BUSY;

	if (pml < 2)
		return ACK_LENGTH;

	if (Sensors == 0)
		return ACK_VOID;

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

	killall (sampling_generator);
	killall (sampling_corrector);
	if (runfsm sampling_generator) {
		if (runfsm sampling_corrector) {
			Status = STATUS_SAMPLING;
			return ACK_OK;
		}
	}

	// We have failed; make sure we are clean
	sampling_stop ();
	Status = STATUS_IDLE;
	return ACK_NORES;
}
