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

static cc1350_rfparams_t RFP = {
	WOR_CYCLE,
	RADIO_LINGER,
	WOR_RSS,
	WOR_PQI
};

void ossint_motion_event (address values, word events) {

	address msg;
	message_report_t *pmt;

	if ((msg = osscmn_xpkt (message_report_code, LastRef,
				sizeof (message_report_t) + 8)) != NULL) {

		pmt = (message_report_t*) osspar (msg);
		pmt->layout = 0x10;
		pmt->data.size = 8;
		((word*)(pmt->data.content)) [0] = values [0];
		((word*)(pmt->data.content)) [1] = values [1];
		((word*)(pmt->data.content)) [2] = values [2];
		((word*)(pmt->data.content)) [3] = events;
		tcv_endpx (msg, YES);
	}
}

fsm ossint_sensor_status_sender {
//
// Message format:
//
//	lword	uptime;
//	lword	seconds;	[uptime + SetTime]
//	lword	taken;		[samples taken]
//	word	battery;
//	word	freemem;
//	word	mnimem;
//	word	rate;		[Samples/Takes per minute]
//	byte	sset;		[ON sensors]
//	byte	status;		[doing what]
//	blob	sstat;		[sensor configuration]
//
	state WAIT_BATTERY:

		word batt, blen;
		address msg;
		message_status_t *pmt;

		read_sensor (WAIT_BATTERY, SENSOR_BATTERY, &batt);

		// Determine the blob length
		blen = sensing_status (NULL);

		if ((msg = osscmn_xpkt (message_status_code, LastRef,
		    sizeof (message_status_t) + blen)) == NULL) {
			// Keep waiting for memory, this will not happen
			delay (256, WAIT_BATTERY);
			release;
		}

		pmt = (message_status_t*) osspar (msg);
		pmt->uptime = seconds ();
		pmt->seconds = pmt->uptime + SetTime;
		pmt->taken = SamplesTaken;
		pmt->battery = batt;
		pmt->freemem = memfree (0, &(pmt->minmem));
		pmt->rate = SamplesPerMinute;
		pmt->sset = Sensors;
		pmt->status = Status;

		pmt->sstat.size = sensing_status (
			(byte*)(((word*)&(pmt->sstat.size)) + 1));

		tcv_endpx (msg, YES);
		finish;
}

void ossint_toggle_radio () {

	byte what;

	if ((what = RadioOn + 1) > 2)
		what = 0;

	osscmn_turn_radio (what);

	led_blink (LED_MAIN, 2 + 2 * what, 128);
}

void ossint_set_radio (const command_radio_t *pmt, word pml) {

	word val;
	byte mod, ope;

	if (pml < 5)
		return ACK_LENGTH;

	mod = NO;

	if (pmt->options > 3)
		return ACK_PARAM;

	if ((val = pmt->worintvl) != 0) {
		// Keep it sane
		if (val < 256)
			val = 256;
		else if (val > 8192)
			val = 8192;
		if (val != RFP.interval) {
			mod = YES;
			RFP.interval = val;
		}
	}

	if ((val = pmt->offdelay) != 0) {
		if (val < 20)
			val = 20;
		if (val != RFP.offdelay) {
			mod = YES;
			RFP.interval = val;
		}
	}

	if (mod)
		tcv_control (RFC, PHYSOPT_SETPARAMS, (address)&RFP);

	runfsm delayed_switch (pmt->options);

	return ACK_OK;
}
