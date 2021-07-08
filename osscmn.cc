/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "osscmn.h"

static cc1350_rfparams_t RFP = {
	WOR_CYCLE,
	RADIO_LINGER,
	WOR_RSS,
	WOR_PQI
};

sint 	RFC;
static byte	LastRef;

byte	RadioOn;

fsm radio_receiver {

	state RS_LOOP:

		address pkt;
		oss_hdr_t *osh;

		pkt = tcv_rnp (RS_LOOP, RFC);

		// NETID is the only identifier
		if (tcv_left (pkt) >= OSSMINPL && pkt [0] == NODE_ID) {
			osh = osshdr (pkt);
			if (osh->code & 0x80) {
				// Unnumbered
				handle_unnumbered_packet (osh->code, osh->ref,
					osspar (pkt),
					tcv_left (pkt) - OSSFRAME);
			} else if (osh->ref != LastRef) {
				LastRef = osh->ref;
				handle_numbered_packet (osh->code,
					osspar (pkt),
					tcv_left (pkt) - OSSFRAME);
			}
		}

		tcv_endp (pkt);
		sameas RS_LOOP;
}

address osscmn_xpkt (byte code, byte ref, word len) {
//
// Tries to acquire a packet for outgoing RF message
//
	address msg;

	if (len & 1)
		// Force the length to be even
		len++;
	if ((msg = tcv_wnp (WNONE, RFC, len + RFPFRAME + sizeof (oss_hdr_t))) !=
	    NULL) {
		osshdr (msg) -> code = code;
		osshdr (msg) -> ref = ref;
	}
	return msg;
}

void osscmn_xack (word status) {

	address msg;

	if ((msg = osscmn_xpkt (0, LastRef, sizeof (status))) != NULL) {
		osspar (msg) [0] = status;
		tcv_endpx (msg, YES);
	}
}

void osscmn_turn_radio (byte on) {

	word par;

	if (RadioOn != on) {

		RadioOn = on;
		if (RadioOn > 1) {
			// Full on
			tcv_control (RFC, PHYSOPT_RXON, NULL);
			return;
		}

		par = RadioOn;
		tcv_control (RFC, PHYSOPT_OFF, &par);
	}
}

void osscmn_setid (word sid) {
//
// Set the node (network) Id
//
	

void osscmn_init () {
//
// Initialize the interface
//
	word sid;

	phys_cc1350 (0, MAX_PACKET_LENGTH);
	tcv_plug (0, &plug_null);

	RFC = tcv_open (NONE, 0, 0);

	// In this app, the role of NETID is played by the host Id
	sid = NODE_ID;
	tcv_control (RFC, PHYSOPT_SETSID, &sid);

	tcv_control (RFC, PHYSOPT_SETPARAMS, (address)&RFP);

	runfsm radio_receiver;

	osscmn_turn_radio (RADIO_MODE_ON);
}
