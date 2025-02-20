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

// ============================================================================
// ============================================================================

static sint		sd_uart;

static oss_hdr_t	*CMD;		// Current command ...
static address		PMT;		// ... and its payload
static word		PML;		// Length

word 			loss_count;	// Lost blocks reported by the Peg

static command_ap_t	APS = { 0, 2, 0 };		// AP status

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

#if ERROR_SIMULATOR

static word		FLoss;

Boolean byte_error (word pl) {

	return FLoss && (((lword) FLoss * pl) >= rnd ());
}

#endif

// ======================

fsm rooster_thread (byte ref) {

	word Counter;

	state RO_START:

		Counter = ACT_WAKE_COUNT;

	state RO_SEND:

		address msg;

		if ((msg = osscmn_xpkt (command_wake_code, ref, 0)) == NULL) {
			delay (1, RO_SEND);
			release;
		}

		tcv_endpx (msg, NO);

		if (--Counter)
			delay (ACT_WAKE_SPACE, RO_SEND);
		else
			finish;
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
				i = (APS.nodeid = pmt->nodeid) & 7;
				tcv_control (RFC, PHYSOPT_SETSID,
					&(APS.nodeid));
				tcv_control (RFC, PHYSOPT_SETCHANNEL, &i);
			}
			done++;
		}
		if (pmt->nretr != BNONE) {
			APS.nretr = pmt->nretr;
			done++;
		}

#if ENABLE_RF_HALT
		if (pmt->halt != BNONE) {
			APS.halt = pmt->halt;
			done++;
		}
#endif

#if ERROR_SIMULATOR
		if (pmt->loss != WNONE) {
			FLoss = pmt->loss;
			done++;
		}
#endif

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
#if ERROR_SIMULATOR
			((message_ap_t*)(msg + 1)) -> loss = FLoss;
#endif
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

	if (CMD->code == command_wake_code) {
		if (!running (rooster_thread))
			runfsm rooster_thread (CMD->ref);
		else 
			oss_ack (ACK_BUSY);
		return;
	}

#undef	msghdr

	// To be passed to the node
	if (CMD->code == command_stream_code)
		// Intercept this one before sending it out
		pegstream_init ();

	i = 0;
	led_tt ();

	if (ENABLE_RF_HALT && APS.halt)
		return;

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

	if (ENABLE_RF_HALT && APS.halt)
		return;

	led_rx ();
	if (code == MESSAGE_CODE_SBLOCK) {
		if (mpl < STRM_NCODES * 4)
			// Ignore garbage
			return;

		pegstream_tally_block (ref, pkt);
	} else if (code == MESSAGE_CODE_ETRAIN) {
		if (mpl < sizeof (message_etrain_t))
			return;

		pegstream_eot (ref, pkt);
		// Piggyback the loss_count onto upper nibble; the Tag only
		// uses three lower flags
		if (loss_count) {
			((message_etrain_t*) pkt) -> flags |= 
				((loss_count > 15) ? 0xf0 :
					((loss_count & 0xf) << 4));
			loss_count = 0;
		}
	} else if (code == message_status_code) {
		// Insert loss_count
		((message_status_t*) pkt) -> ploss = loss_count;
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

		si = (APS.nodeid = GROUP_ID) & 7;

		led_signal (0, 4, 128);

		phys_uart (1, OSS_PACKET_LENGTH, 0);

		osscmn_init ();
		// Set channel to match the assumed (initial) node Id
		tcv_control (RFC, PHYSOPT_SETCHANNEL, &si);

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
