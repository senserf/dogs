/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef	__pg_display_h
#define	__pg_display_h

//+++ "display.cc"

#define	DEBUGGING

#include "sysio.h"
#include "ez430_lcd.h"
#include "watch.h"

typedef	struct {
//
// Describes the content of a display line; probably an overkill; we shall
// simplify it later once we get it to work
//
	time_callback_f	cb_sec;
	time_callback_f	cb_min;
	void	(*fn_show) ();
	void	(*fn_clear) ();
	byte	where;

} line_display_t;

// ============================================================================

#define	N_DISPLAYS		13
#define	N_WINDOWS		2

#define	DISPLAY_MWATCH		0
#define	DISPLAY_SWATCH		1
#define	DISPLAY_CWATCH		2
#define	DISPLAY_MWATCH_SET	3
#define	DISPLAY_SWATCH_SET	4
#define	DISPLAY_CWATCH_SET	5
#define	DISPLAY_GSTAT		6
#define	DISPLAY_GSTAT_SET	7
#define	DISPLAY_RADIO		8
#define	DISPLAY_GLNUM		9
#define	DISPLAY_GWNUM		10
#define	DISPLAY_TEMP		11
#define	DISPLAY_ACCEL		12

//
// Items than can be displayed, typically an item is a specific combo of two
// lines
//
#define	ITEM_WATCH		0
#define ITEM_RADIO		1
#define	ITEM_ACCEL		2
#define	ITEM_PRESS		3

#define	N_ITEMS			4

typedef struct {
	void	(*fn_display) ();
	void	(*fn_option) (sint);
	Boolean	(*fn_enter_set_mode) ();
	void	(*fn_exit_set_mode) ();
	void	(*fn_set_next) ();
	void	(*fn_set_value) (sint);
} item_t;

// ============================================================================
// Buttons; we only use four
// ============================================================================

#define	BUTTON_LT		0	// Left Top
#define	BUTTON_LB		1	// Left Bottom
#define	BUTTON_RT		2	// Right Top	aka >
#define	BUTTON_RB		3	// Right Bottom	aka <

#define	N_BUTTONS		4

#define	DEPRESS_CHECK_INTERVAL	16

#define	LONG_PRESS_TIME		((3 * 1024) / DEPRESS_CHECK_INTERVAL)

// ============================================================================
// Requests to the accelerator thread (bit flags)
// ============================================================================

#define	ACCRQ_ON		0x01
#define	ACCRQ_OFF		0x02
#define	ACCRQ_VALUE		0x04

// ============================================================================

extern byte	RadioOn;
extern byte	AccelOn;
extern byte	WatchBeingSet;
extern lword	AccelEvents, AccelTotal;
extern word	AccelLast, AccelMax;

void display_clear (byte);

void watch_display (), watch_option (sint), watch_exit_set_mode ();
void watch_set_next (), watch_set_value (sint);
Boolean watch_enter_set_mode ();

void radio_display (), radio_exit_set_mode (), radio_set_value (sint);
Boolean radio_enter_set_mode ();

void accel_display (), accel_option (sint), accel_exit_set_mode ();
void accel_set_value (sint);
Boolean accel_enter_set_mode ();

void press_display ();

void switch_radio (byte), display_rfstat (), accel_update_display ();
void accel_clear ();

Boolean accel_start (Boolean, lword, lword, byte, word), press_trigger (byte);

void press_update_display (lword, sint);

// ============================================================================
#ifdef DEBUGGING
void rms (word, word, word);
#else
#define	rms (a,b,c)	do { } while (0)
#endif
// ============================================================================

#endif
