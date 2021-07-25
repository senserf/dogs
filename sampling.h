/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef	__pg_sampling_h
#define	__pg_sampling_h

//+++ sampling.cc

#include "sysio.h"

#define	MAX_SAMPLES_PER_MINUTE	(256 * 60)
#define	MIN_SAMPLES_PER_MINUTE	1

// This is for adjusting the interval to meet the long-term rate (assuming that
// short-term departures are OK)
#define	MAX_SAMPLE_SPACE	(63 * 1024)
#define	MIN_SAMPLE_SPACE	3

#define	STATUS_SAMPLING		1
#define	STATUS_STREAMING	2

extern word SamplesPerMinute, SampleSpace;
extern lword SampleStartSecond, SamplesTaken;
extern byte Status;

word sampling_start (const command_sample_t*, word);
void sampling_stop ();

fsm sampling_corrector;

#endif
