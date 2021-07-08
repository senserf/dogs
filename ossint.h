/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef	__pg_ossint_h
#define	__pg_ossint_h

//+++ ossint.cc

#include "ossi.h"
#include "osscmn.h"

// RF params (mutable)
#define	WOR_CYCLE		1024			// 1 second
#define	RADIO_LINGER		(5 * 1024)		// 5 seconds
#define	WOR_RSS			20
#define	WOR_PQI			YES

#define	RADIO_MODE_HIBERNATE	3
#define	RADIO_MODE_ON		2
#define	RADIO_MODE_WOR		1
#define RADIO_MODE_OFF		0

// Block messages; the last-block (blast) message can include a block or be
// empty
#define	message_block_code	128
#define	message_blast_code	129

void ossint_toggle_radio ();
void ossint_set_radio (const command_radio_t*, word);
void ossint_motion_event (address, word);

// Provided by the application
void handle_unnumbered_packet (byte, byte, const address, int);
void handle_numbered_packet (byte const address, int);

fsm ossint_sensor_status_sender;

#endif
