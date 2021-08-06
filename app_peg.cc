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

#include "ledsignal.h"

#ifndef	__SMURPH__
#if RADIO_WOR_MODE
#define	USE_WAKE_ON_RADIO	1
#endif
#endif

#ifndef	USE_WAKE_ON_RADIO
#define	USE_WAKE_ON_RADIO	0
#endif

// ============================================================================
// ============================================================================

static sint		sd_uart;

static oss_hdr_t	*CMD;		// Current command ...
static address		PMT;		// ... and its payload
static word		PML;		// Length

static command_ap_t	APS = { 1, 2, 0 };		// AP status

// Debugging ==================================================================
static word		FLoss = 0;
// End debugging ==============================================================

// ============================================================================

static void led_hb () {
	// Two quick blinks on heartbeat
	led_signal (0, 2, 256);
}

static void led_tt () {
	// One longish blink on outgoing message
	led_signal (0, 1, 768);
}

static void led_rx () {
	led_signal (1, 1, 16);
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

#if USE_WAKE_ON_RADIO
	byte WorCnt = 0;
#endif

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
		if (pmt->nretr != BNONE) {
			APS.nretr = pmt->nretr;
			done++;
		}

		// Debugging ==================================================
		if (pmt->loss != WNONE) {
			FLoss = pmt->loss;
			done++;
		}
		// End debugging ==============================================

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
			// Debugging ==========================================
			((message_ap_t*)(msg + 1)) -> loss = FLoss;
			// End debugging ======================================
			tcv_endp (msg);
		}
		led_tt ();
		return;
#undef	pmt
	}

	if (PML > MAX_PACKET_LENGTH - PKT_FRAME_ALL) {
		// Too long for radio
		oss_ack (ACK_AP_TOOLONG);
		return;
	}

#if USE_WAKE_ON_RADIO
	if (CMD->code == command_radio_code) {
#define	pmt	((command_radio_t*)PMT)
		if (pmt->options == 2) {
			// On request, force wor
			WorCnt = 1;
		}
	}
#undef	pmt
#endif

#undef	msghdr

	// To be passed to the node
	if (CMD->code == command_stream_code)
		// Intercept this one for a trifle
		pegstream_init ();

#if USE_WAKE_ON_RADIO
	while (WorCnt) {
		if ((msg = tcv_wnpu (WNONE, RFC, PML + PKT_FRAME_ALL)) == NULL)
			return;
		memcpy (msg + (PKT_FRAME_PHDR/2), CMD, PML + PKT_FRAME_OSS);
		tcv_endpx (msg, NO);
		WorCnt--;
	}
	led_tt ();
#endif

	i = 0;
	led_tt ();

	do {
		if ((msg = tcv_wnp (WNONE, RFC, PML + PKT_FRAME_ALL)) == NULL)
			return;
		memcpy (msg + (PKT_FRAME_PHDR/2), CMD, PML + PKT_FRAME_OSS);
		tcv_endpx (msg, YES);
		i++;
	} while (i < APS.nretr);
}

void handle_rf_packet (byte code, byte ref, address pkt, word mpl) {
//
// Just pass it to the OSS
//
	address msg;

	led_rx ();
	if (code == MESSAGE_CODE_SBLOCK) {
		if (mpl < STRM_NCODES * 4)
			// Ignore garbage
			return;

		// Debugging ==================================================
		if (FLoss && (rnd () & 0x3ff) < FLoss) return;
		// End debugging ==============================================

		pegstream_tally_block (ref, pkt);
	} else if (code == MESSAGE_CODE_ETRAIN) {
		if (mpl < sizeof (message_etrain_t))
			return;
		pegstream_eot (ref, pkt);
	}

	// Pass to OSS, include the RSSI
	if ((msg = tcv_wnp (WNONE, sd_uart, upl (mpl) + 2 + 2)) != NULL) {
		((oss_hdr_t*)msg)->code = code;
		((oss_hdr_t*)msg)->ref = ref;
		memcpy (msg + (PKT_FRAME_OSS/2), pkt, mpl + 2);
		tcv_endp (msg);
	}
}
				
fsm root {

	state RS_INIT:

		word si;

		APS.nodeid = GROUP_ID;

		led_signal (0, 4, 128);

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
