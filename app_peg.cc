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
#include "cc1350.h"

#include "osscmn.h"
#include "pegstream.h"

// UART packet length as a function of the payload length (2 bytes for CRC)
#define	upl(a)		((a) + 2)

#include "ledblink.h"

// ============================================================================
// ============================================================================

static sint		sd_uart;

static oss_hdr_t	*CMD;		// Current command ...
static address		PMT;		// ... and its payload
static word		PML;		// Length

static command_ap_t	APS = { 1, 1024, 0, 2 };		// AP status

// ============================================================================

static void led_hb () {
	// Two quick blinks on heartbeat
	led_blink (0, 2, 256);
}

static void led_tt () {
	// One longish blink on outgoing message
	led_blink (0, 1, 768);
}

// ============================================================================

static void oss_ack (word status) {
//
// ACK to the OSS
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

static void handle_oss_command () {
//
// Process a command arriving from the OSS
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
			msg [1] = (word)(OSS_PRAXIS_ID ^ (OSS_PRAXIS_ID >> 16));
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
			if (pmt->nodeid != 0) {
				// Request to set group Id
				APS.nodeid = pmt->nodeid;
				osscmn_rfcontrol (PHYSOPT_SETSID,
					&(APS.nodeid));
			}
			done++;
		}
		if (pmt->worprl != WNONE) {
			if (RFP.interval != (APS.worprl = pmt->worprl)) {
				RFP.interval = pmt->worprl;
				osscmn_rfcontrol (PHYSOPT_SETPARAMS,
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

#ifndef	__SMURPH__
	// No WOR in VUEE
	for (i = 0; i < APS.nworp; i++) {
		// This is executed if WOR wakeup is set, nworp times
		if ((msg = tcv_wnpu (WNONE, RFC,
		    PML + sizeof (oss_hdr_t) + RFPFRAME)) == NULL)
			return;
		memcpy (msg + (RFPHDOFF/2), CMD, PML + sizeof (oss_hdr_t));
		tcv_endpx (msg, NO);
	}
#endif
	i = 0;
	led_tt ();
	do {
#ifndef	__SMURPH__
		if (APS.norp == 0 && APS.nworp != 0)
			// WOR wakeup has been sent, and we don't want to
			// force regular packets on top of it
			return;
#endif
		// At least once if APS.nworp == 0, ragrdless of the setting of
		// norp
		if ((msg = tcv_wnp (WNONE, RFC,
		    PML + sizeof (oss_hdr_t) + RFPFRAME)) == NULL)
			return;
		memcpy (msg + (RFPHDOFF/2), CMD, PML + sizeof (oss_hdr_t));
		tcv_endpx (msg, YES);
		i++;
	} while (i < APS.norp);
}

void handle_rf_packet (byte code, byte ref, address pkt, word mpl) {
//
// Just pass it to the OSS
//
	address msg;

	if (code == MESSAGE_CODE_SBLOCK) {
		if (mpl != STRM_NCODES * 4)
			// Ignore garbage
			return;
		pegstream_tally_block (ref, pkt);
	} else if (code == MESSAGE_CODE_ETRAIN) {
		if (mpl == sizeof (streot_t))
			pegstream_eot (ref, pkt);
		// Do not pass to OSS
		return;
	}

	// Pass to OSS

	if ((msg = tcv_wnp (WNONE, sd_uart, upl (mpl) + 2)) != NULL) {
		((byte*)msg) [0] = code;
		((byte*)msg) [1] = ref;
		memcpy (msg + (RFPHDOFF/2), pkt, mpl);
		tcv_endp (msg);
	}
}
				
fsm root {

	state RS_INIT:

		word si;

		APS.nodeid = GROUP_ID;

		// Indicates the AP is plugged in
		led_init (2);
		leds (0, 1);

		phys_uart (1, OSS_PACKET_LENGTH, 0);

		osscmn_init ();

		sd_uart = tcv_open (WNONE, 1, 0);	// The UART
		si = 0xffff;
		tcv_control (sd_uart, PHYSOPT_SETSID, &si);

	state RS_LOOP:

		CMD = (oss_hdr_t*) tcv_rnp (RS_LOOP, sd_uart);
		PML = tcv_left ((address)CMD);

		if (PML >= sizeof (oss_hdr_t)) {
			PML -= sizeof (oss_hdr_t);
			PMT = (address)(CMD + 1);
			handle_oss_command ();
		} else {
			oss_ack (ACK_AP_BAD);
		}

		tcv_endp ((address)CMD);
		proceed RS_LOOP;
}
