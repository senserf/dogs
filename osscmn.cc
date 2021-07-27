/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "osscmn.h"

cc1350_rfparams_t RFP = {
	WOR_CYCLE,
	RADIO_LINGER,
	WOR_RSS,
	WOR_PQI
};

sint 	RFC;

byte	RadioOn, LastRef;

fsm radio_receiver {

	state RS_LOOP:

		address pkt;
		oss_hdr_t *osh;

		pkt = tcv_rnp (RS_LOOP, RFC);

		// NETID is the only identifier
		if (tcv_left (pkt) >= OSSMINPL) {
			osh = osshdr (pkt);
			handle_rf_packet (osh->code, osh->ref, 
				osspar (pkt), tcv_left (pkt) - OSSFRAME);
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

void osscmn_xack (byte ref, word status) {

	address msg;

	if ((msg = osscmn_xpkt (0, ref, sizeof (status))) != NULL) {
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
			osscmn_rfcontrol (PHYSOPT_RXON, NULL);
			return;
		}

		par = RadioOn;
		osscmn_rfcontrol (PHYSOPT_OFF, &par);
	}
}

void osscmn_init () {
//
// Initialize the interface
//
	word sid;

	phys_cc1350 (0, MAX_PACKET_LENGTH);
	tcv_plug (0, &plug_null);

	RFC = tcv_open (NONE, 0, 0);

	// In this app, the role of NETID is played by the host Id
	sid = GROUP_ID;
	osscmn_rfcontrol (PHYSOPT_SETSID, &sid);
	osscmn_rfcontrol (PHYSOPT_SETPARAMS, (address)&RFP);

	runfsm radio_receiver;

	osscmn_turn_radio (RADIO_MODE_ON);
}

void osscmn_rfcontrol (sint op, address pmt) {
//
// Issue sysopt to RF (account for VUEE)
//
#ifdef __SMURPH__
	if (op == PHYSOPT_SETPARAMS) {
#if RADIO_WOR_MODE
		emul (1, "RF SETPARAMS: off=%1u, int=%1u, rss=%1u, pqt=%1u",
			((cc1350_rfparams_t*)pmt)->offdelay,
			((cc1350_rfparams_t*)pmt)->interval,
			((cc1350_rfparams_t*)pmt)->rss,
			((cc1350_rfparams_t*)pmt)->pqt);
#else
		emul (1, "RF SETPARAMS: off=%1u",
			((cc1350_rfparams_t*)pmt)->offdelay);
#endif
		return;
	}
	if (op == PHYSOPT_OFF) {
		if (pmt != NULL && pmt) {
			// WOR request, make it simply ON
			tcv_control (RFC, PHYSOPT_RXON, NULL);
			emul (1, "RF WOR MODE");
			return;
		}
		// Fall through for OFF
	}
#endif
	tcv_control (RFC, op, pmt);
};
