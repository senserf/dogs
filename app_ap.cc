/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "sysio.h"
#include "phys_cc1350.h"
#include "phys_uart.h"
#include "plug_null.h"

#include "ossi.h"
#include "rf.h"
#include "netid.h"

#include "cc1350.h"

// UART packet length as a function of the payload length (2 bytes for CRC)
#define	upl(a)		((a) + 2)

#define	SIGNALLING_LEDS		2	// Two leds

#include "leddefs.h"

// ============================================================================
// ============================================================================

static sint		sd_rf, sd_uart;

static oss_hdr_t	*CMD;		// Current command ...
static address		PMT;		// ... - the header
static word		PML;		// Length
static byte		LastRef = 0;

static command_ap_t	APS = { 1, 1024, 0, 2 };		// AP status

// The AP never goes into WOR, so we basically need the second entry only
static cc1350_rfparams_t	RFP = { 4096, 1024, 20, 1 };

// ============================================================================

static void led_hb () {
	// Two quick blinks on heartbeat
	blink (0, 2, 256);
}

static void led_tt () {
	// One longish blink on outgoing message
	blink (0, 1, 768);
}

static void led_ft () {
	// One quick blink on incoming message
	blink (1, 1, 512);
}

// ============================================================================

static void oss_ack (word status) {
//
// ACK to the interface
//
	address msg;

	if ((msg = tcv_wnp (WNONE, sd_uart,
	    upl (sizeof (oss_hdr_t) + sizeof(status)))) != NULL) {
		((oss_hdr_t*)msg)->code = 0;
		((oss_hdr_t*)msg)->ref = CMD->ref;
		msg [1] = status;
		tcv_endp (msg);
	}
}

static void handle_command () {
//
// Process a command arriving from the interface
//
	address msg;
	word i;

#define	msghdr	((oss_hdr_t*)msg)

	if (CMD->code == 0) {
		// Heartbeat/autoconnect
		if (*((lword*)PMT) == OSS_PRAXIS_ID && (msg = 
		    tcv_wnp (WNONE, sd_uart,
		    upl (sizeof (oss_hdr_t) + sizeof (word)))) != NULL) {
			// Heartbeat response		
			msg [0] = 0;
			*(msg + 1) = (word)(OSS_PRAXIS_ID ^ 
				(OSS_PRAXIS_ID >> 16));
			tcv_endp (msg);
			led_hb ();
		}
		return;
	}

	if (CMD->code == command_ap_code) {
		word rfp [2];
		word done;
		// Command addressed to the AP
		if (PML < sizeof (command_ap_t)) {
			oss_ack (ACK_AP_FMT);
			return;
		}
		if (CMD->ref == LastRef)
			// Quietly ignore repeats
			return;

#define	pmt	((command_ap_t*)PMT)
		done = 0;
		if (pmt->nodeid != WNONE) {
			if (pmt->nodeid != 0)
				// Request to set NodeId
				APS.nodeid = pmt->nodeid;
			done++;
		}
		if (pmt->worprl != WNONE) {
			if (RFP.interval != (APS.worprl = pmt->worprl)) {
				RFP.interval = pmt->worprl;
				tcv_control (sd_rf, PHYSOPT_SETPARAMS,
					(address)&RFP);
			}
			done++;
		}
		if (pmt->nworp != BNONE) {
			APS.nworp = pmt->nworp;
			done++;
		}
		if (pmt->norp != BNONE) {
			APS.norp = pmt->norp;
			done++;
		}
		if (done) {
			oss_ack (ACK_OK);
			LastRef = CMD->ref;
		} else {
			// Report the parameters
			if ((msg = tcv_wnp (WNONE, sd_uart,
			    upl (sizeof (oss_hdr_t) + sizeof (message_ap_t))))
			    == NULL)
				return;
			LastRef = CMD->ref;
			msghdr->code = message_ap_code;
			msghdr->ref = CMD->ref;
			memcpy (msg + 1, &APS, sizeof (message_ap_t));
			tcv_endp (msg);
		}
		led_tt ();
		return;
#undef	pmt
#undef	msghdr
	}

	if (PML > MAX_PACKET_LENGTH - RFPFRAME - sizeof (oss_hdr_t)) {
		// Too long for radio
		oss_ack (ACK_AP_TOOLONG);
		return;
	}

	// To be passed to the node

	for (i = 0; i < APS.nworp; i++) {
		// This is executed if WOR wakeup is set, nworp times
		if ((msg = tcv_wnpu (WNONE, sd_rf,
		    PML + sizeof (oss_hdr_t) + RFPFRAME)) == NULL)
			return;
		msg [1] = APS.nodeid;
		memcpy (msg + (RFPHDOFF/2), CMD, PML + sizeof (oss_hdr_t));
		tcv_endp (msg);
	}

	i = 0;
	led_tt ();
	do {
		if (APS.norp == 0 && APS.nworp != 0)
			// WOR wakeup has been sent, and we don't want to
			// force regular packets on top of it
			return;
		// At least once if APS.nworp == 0, ragrdless of the setting of
		// norp
		if ((msg = tcv_wnp (WNONE, sd_rf,
		    PML + sizeof (oss_hdr_t) + RFPFRAME)) == NULL)
			return;
		msg [1] = APS.nodeid;
		memcpy (msg + (RFPHDOFF/2), CMD, PML + sizeof (oss_hdr_t));
		tcv_endp (msg);
		i++;
	} while (i < APS.norp);
}

fsm receiver {

	address pkt, msg;

	state RCV_WAIT:

		word len;

		pkt = tcv_rnp (RCV_WAIT, sd_rf);
		len = tcv_left (pkt);

		if (len >= RFPFRAME + sizeof (oss_hdr_t) + 2 &&
		  len <= OSS_PACKET_LENGTH + RFPFRAME &&
		    pkt [1] == APS.nodeid) {
			// Write to the UART; include RSS and LQI
			len -= RFPHDOFF;
			if ((msg = tcv_wnp (WNONE, sd_uart, upl (len)))
			    != NULL) {
				memcpy (msg, pkt + (RFPHDOFF/2), len);
				tcv_endp (msg);
				led_ft ();
			}
		}

		tcv_endp (pkt);
		proceed RCV_WAIT;
}
				
fsm root {

	state RS_INIT:

		// Indicates the AP is plugged in
		leds (0, 1);

		word si = 0xFFFF;

		phys_cc1350 (0, MAX_PACKET_LENGTH);
		phys_uart (1, OSS_PACKET_LENGTH, 0);
		tcv_plug (0, &plug_null);

		sd_rf = tcv_open (WNONE, 0, 0);		// The radio
		sd_uart = tcv_open (WNONE, 1, 0);	// The UART

		tcv_control (sd_uart, PHYSOPT_SETSID, &si);

		si = NETID;
		tcv_control (sd_rf, PHYSOPT_SETSID, &si);
		tcv_control (sd_rf, PHYSOPT_SETPARAMS, (address)&RFP);
		tcv_control (sd_rf, PHYSOPT_ON, NULL);

		if (sd_rf < 0 || sd_uart < 0)
			syserror (ERESOURCE, "ini");

		runfsm receiver;

	state RS_LOOP:

		CMD = (oss_hdr_t*) tcv_rnp (RS_LOOP, sd_uart);
		PML = tcv_left ((address)CMD);

		if (PML >= sizeof (oss_hdr_t)) {
			PML -= sizeof (oss_hdr_t);
			PMT = (address)(CMD + 1);
			handle_command ();
		} else {
			oss_ack (ACK_AP_BAD);
		}

		tcv_endp ((address)CMD);
		proceed RS_LOOP;
}
