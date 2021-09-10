/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef	__pg_pegstream_h
#define	__pg_pegstream_h

//+++ pegstream.cc

#include "osscmn.h"

void pegstream_init ();
void pegstream_tally_block (byte, address);
void pegstream_eot (byte, address);

extern word loss_count;

#endif

