/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef	__pg_streaming_h
#define	__pg_streaming_h

#include "ossint.h"
#include "sensing.h"
#include "sampling.h"

word streaming_start (const command_sample_t*, word);
void streaming_stop ();
void streaming_tack (byte, byte*, word);

#endif
