/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "osscmn.h"

sint 	RFC;
byte	LastRef;

#ifdef	TAG_DEVICE
// RF duty cycling
extern	byte RadioActiveCD;
#define	mark_active	RadioActiveCD = 0
#else
#define	mark_active	CNOP
#endif

fsm radio_receiver {

	state RS_LOOP:

		address pkt;
		oss_hdr_t *osh;

		pkt = tcv_rnp (RS_LOOP, RFC);
		mark_active;

		if (tcv_left (pkt) >= PKT_FRAME_ALL) {
			osh = pkt_osshdr (pkt);
			handle_rf_packet (osh->code, osh->ref, 
				pkt_payload (pkt),
					tcv_left(pkt) - PKT_FRAME_ALL);
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
	if ((msg = tcv_wnp (WNONE, RFC, len + PKT_FRAME_ALL)) != NULL) {
		pkt_osshdr (msg) -> code = code;
		pkt_osshdr (msg) -> ref = ref;
	}
	mark_active;
	return msg;
}

void osscmn_xack (byte ref, word status) {

	address msg;

	if ((msg = osscmn_xpkt (0, ref, sizeof (status))) != NULL) {
		pkt_payload (msg) [0] = status;
		tcv_endpx (msg, YES);
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

	tcv_control (RFC, PHYSOPT_SETSID, &sid);

	runfsm radio_receiver;

	tcv_control (RFC, PHYSOPT_RXON, NULL);
}
