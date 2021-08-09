/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "tag.h"
#include "ossint.h"
#include "sysio.h"
#include "cc1350.h"
#include "tcvphys.h"
#include "phys_cc1350.h"
#include "plug_null.h"
#include "rf.h"
#include "sensing.h"
#include "sampling.h"
#include "ledsignal.h"

void ossint_motion_event (address values, word events) {

	address msg;
	message_motion_t *pmt;

	if ((msg = osscmn_xpkt (message_motion_code, LastRef,
				sizeof (message_motion_t))) != NULL) {

		pmt = (message_motion_t*) pkt_payload (msg);
		pmt->accel [0] = values [0];
		pmt->accel [1] = values [1];
		pmt->accel [2] = values [2];
		pmt->events = events;
		tcv_endpx (msg, YES);
	}
}

word ossint_send_status () {
//
// Message format:
//
//	lword	uptime;
//	lword	taken;		[samples taken]
//	word	freemem;
//	word	mnimem;
//	word	rate;		[Samples/Takes per minute]
//	byte	battery;
//	byte	sset;		[ON sensors]
//	byte	status;		[doing what]
//	blob	sstat;		[sensor configuration]
//
	word blen;
	address msg;
	message_status_t *pmt;

	// The blob length
	blen = sensing_status (NULL);

	if ((msg = osscmn_xpkt (message_status_code, LastRef,
	    sizeof (message_status_t) + blen)) == NULL)
		return ACK_NORES;

	pmt = (message_status_t*) pkt_payload (msg);
	pmt->uptime = seconds ();
	pmt->taken = SamplesTaken;
	pmt->battery = VOLTAGE;
	pmt->freemem = memfree (0, &(pmt->minmem));
	pmt->rate = SamplesPerMinute;
	pmt->status = Status;
	pmt->sset = Sensors;

	pmt->sstat.size = sensing_status (
		(byte*)(((word*)&(pmt->sstat.size)) + 1));
	tcv_endpx (msg, YES);

	return ACK_OK;
}
