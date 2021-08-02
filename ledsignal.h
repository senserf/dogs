/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef __pg_ledsignal_h
#define	__pg_ledsignal_h

#include "sysio.h"

#ifndef	N_SIGNAL_LEDS
#define	N_SIGNAL_LEDS 0
#endif

#if N_SIGNAL_LEDS > 0

//+++ "ledsignal.cc"

void led_signal (word, word, word);
void led_stop ();

#else

#define	led_signal(a,b,c)	CNOP
#define	led_stop()		CNOP

#endif

#endif
