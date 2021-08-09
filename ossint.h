/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef	__pg_ossint_h
#define	__pg_ossint_h

//+++ ossint.cc

// Used by the Tag only

#include "ossi.h"
#include "osscmn.h"

// Block messages; the last-block (blast) message can include a block or be
// empty
#define	message_block_code	128
#define	message_blast_code	129

void ossint_motion_event (address, word);
word ossint_send_status ();

#endif
