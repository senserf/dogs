/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "display.h"

// ============================================================================

static void s2d (word seg, byte w, Boolean blink) {
//
// Display two digits in the indicated segment, optionally blinking
//
	word attr = blink ? LCD_MODE_BLINK : LCD_MODE_SET;
	ezlcd_item (seg++, (w % 10) | attr);
	w /= 10;
	if (seg == LCD_SEG_L2_5 || seg == LCD_SEG_L1_3) {
		// In these segments, zero is blank
		if (!w)
			attr = LCD_MODE_CLEAR;
	}
	ezlcd_item (seg, w | attr);
}

static void stxt (word seg, char *txt, Boolean blink) {

	word attr = blink ? LCD_MODE_BLINK : LCD_MODE_SET;

	while (*txt != '\0')
		ezlcd_item (seg--, (word)((byte)(*txt++ - 'A' + 10)) | attr);
}

static void clear_lcd (word s, word t) {

	while (1) {
		ezlcd_item (s, LCD_MODE_CLEAR);
		if (s == t)
			return;
		if (s < t)
			s++;
		else
			s--;
	}
}

static void display (byte);
static byte display_content (byte);

// ============================================================================
// Variables (grouped together for defragmentation)
// ============================================================================

static lword GLNum;		// Generic number long
static word GWNum;		// Generic number short
static sint LTemp;		// Temperature (can be negative)
static rtc_time_t SRTC;		// SRTC <- RTC before setting
static Boolean W24H;		// Flag == 24h clock
static Boolean w24h;		// For setting the watch
static byte GStat;		// Generic status
static Boolean GLNPer;		// Generic number long (decimal dot)
static byte AcsMode;		// Accelerator display mode

// ============================================================================
// The watch module
// ============================================================================

static void watch_mn_show () {
//
// Minutes in the upper row
//
	word am, pm, hr;

	// Make the colon blink
	ezlcd_item (LCD_SEG_L1_COL, LCD_MODE_BLINK);

	hr = RTC.hour;
	am = pm = LCD_MODE_CLEAR;

	if (!W24H) {
		pm = LCD_MODE_SET;
		if (hr > 12)
			hr -= 12;
		else
			am = LCD_MODE_SET;
	}

	ezlcd_item (LCD_SYMB_AM, am);
	ezlcd_item (LCD_SYMB_PM, pm);

	s2d (LCD_SEG_L1_2, (byte)hr, NO);
	s2d (LCD_SEG_L1_0, RTC.minute, NO);
}

static void watch_mn_clear () {

	clear_lcd (LCD_SEG_L1_COL, LCD_SEG_L1_COL);
	clear_lcd (LCD_SYMB_AM, LCD_SYMB_PM);
	clear_lcd (LCD_SEG_L1_0, LCD_SEG_L1_3);
}

static void watch_mn_update () {
//
// To be queued for 1 min updates
//
	if (RTC.minute == 0)
		watch_mn_show ();
	else
		s2d (LCD_SEG_L1_0, RTC.minute, NO);
}

// Option: seconds

static void watch_sc_show () {
//
// Seconds in second row
//
	s2d (LCD_SEG_L2_0, RTC.second, NO);
}

static void watch_sc_clear () {

	clear_lcd (LCD_SEG_L2_0, LCD_SEG_L2_1);
}

// Option: calendar

static void watch_ca_show () {
//
// Calendar in second row
//
	ezlcd_item (LCD_SEG_L2_COL0, LCD_MODE_SET);
	ezlcd_item (LCD_SEG_L2_COL1, LCD_MODE_SET);
	s2d (LCD_SEG_L2_4, RTC.month, NO);
	s2d (LCD_SEG_L2_2, RTC.day, NO);
	s2d (LCD_SEG_L2_0, RTC.year, NO);
}

static void watch_ca_clear () {

	clear_lcd (LCD_SEG_L2_COL1, LCD_SEG_L2_COL0);
	clear_lcd (LCD_SEG_L2_0, LCD_SEG_L2_5);
}

static void watch_ca_update () {
//
// Every minute
//
	if (RTC.hour == 0 && RTC.minute == 0)
		// The only moment when the calendar changes
		watch_ca_show ();
}

// ============================================================================
// Setting mode for the watch module
// ============================================================================

static void watch_mns_show () {
//
// Minute set
//
	word am, pm, hr;

	// Solid, no blinking
	ezlcd_item (LCD_SEG_L1_COL, LCD_MODE_SET);

	ezlcd_item (LCD_SYMB_ARROW_UP,
		WatchBeingSet == 2 ? LCD_MODE_BLINK : LCD_MODE_CLEAR);

	hr = SRTC.hour;
	am = pm = LCD_MODE_CLEAR;

	if (!w24h) {
		pm = LCD_MODE_SET;
		if (hr > 12)
			hr -= 12;
		else
			am = LCD_MODE_SET;
	}

	if (WatchBeingSet == 2) {
		am |= LCD_MODE_BLINK;
		pm |= LCD_MODE_BLINK;
	} 

	ezlcd_item (LCD_SYMB_AM, am);
	ezlcd_item (LCD_SYMB_PM, pm);

	s2d (LCD_SEG_L1_2, (byte)hr, WatchBeingSet == 3);
	s2d (LCD_SEG_L1_0, SRTC.minute, WatchBeingSet == 1);
}

static void watch_mns_clear () {

	watch_mn_clear ();
	ezlcd_item (LCD_SYMB_ARROW_UP, LCD_MODE_CLEAR);
}

static Boolean watch_mns_button (sint upd) {

	switch (WatchBeingSet) {

		case 1:

			upd += SRTC.minute;
			if (upd < 0)
				upd = 59;
			else if (upd > 59)
				upd = 0;

			SRTC.minute = (byte)upd;

			break;

		case 2:

			if (w24h) {
				if (SRTC.hour >= 12) 
					SRTC.hour -= 12;
				w24h = NO;
				if (upd > 0)
					SRTC.hour += 12;
			} else if (upd > 0) {
				if (SRTC.hour >= 12)
					w24h = YES;
				else
					SRTC.hour += 12;
			} else {
				if (SRTC.hour >= 12)
					SRTC.hour -= 12;
				else
					w24h = YES;
			}

			break;

		case 3:

			upd += SRTC.hour;
			if (upd < 0)
				upd = 23;
			else if (upd > 23)
				upd = 0;

			SRTC.hour = (byte)upd;

			break;

		default:

			return NO;
	}

	watch_mns_show ();

	return YES;
}

// ============================================================================
// Seconds watch set
// ============================================================================

static void watch_ses_show () {

	s2d (LCD_SEG_L2_0, SRTC.second, WatchBeingSet == 0);
}

static Boolean watch_ses_button (sint upd) {

	upd += SRTC.second;
	if (upd < 0)
		upd = 59;
	else if (upd > 59)
		upd = 0;

	SRTC.second = (byte) upd;

	watch_ses_show ();

	return YES;
}

static void watch_cas_show () {

	ezlcd_item (LCD_SEG_L2_COL0, LCD_MODE_SET);
	ezlcd_item (LCD_SEG_L2_COL1, LCD_MODE_SET);
	s2d (LCD_SEG_L2_4, SRTC.month, WatchBeingSet == 5);
	s2d (LCD_SEG_L2_2, SRTC.day, WatchBeingSet == 4);
	s2d (LCD_SEG_L2_0, SRTC.year, WatchBeingSet == 6);
}

static Boolean watch_cas_button (sint upd) {

	sint max;

	switch (WatchBeingSet) {

		case 4:

			if (SRTC.month == 2) {
				max = 28;
				if ((SRTC.year & 0x3) == 0)
					max++;
			} else {
				if ((SRTC.month > 7 && (SRTC.month & 1) == 0)
				 || (SRTC.month < 8 && (SRTC.month & 1) != 0))
					max = 31;
				else
					max = 30;
			}

			upd += SRTC.day;
			if (upd <= 0)
				upd = max;
			else if (upd > max)
				upd = 1;

			SRTC.day = (byte) upd;

			break;

		case 5:

			upd += SRTC.month;
			if (upd > 12)
				upd = 1;
			else if (upd < 1)
				upd = 12;

			SRTC.month = (byte) upd;

			break;

		case 6:

			upd += SRTC.year;
			if (upd > 99)
				upd = 14;
			else if (upd < 14)
				upd = 99;

			SRTC.year = (byte) upd;

			break;

		default:

			return NO;
	}

	watch_cas_show ();
	return YES;
}

// ============================================================================

void watch_display () {

	display (DISPLAY_MWATCH);
	display (DISPLAY_CWATCH);
}

void watch_option (sint dir) {

	display ((display_content (1) == DISPLAY_SWATCH) ?
		DISPLAY_CWATCH : DISPLAY_SWATCH);
}

Boolean watch_enter_set_mode () {

	memcpy (&SRTC, &RTC, sizeof (SRTC));
	w24h = W24H;

	WatchBeingSet = 0;
	display (DISPLAY_MWATCH_SET);
	display (DISPLAY_SWATCH_SET);
	return YES;
}

void watch_exit_set_mode () {

	WatchBeingSet = BNONE;

	memcpy (&RTC, &SRTC, sizeof (RTC));
	rtc_set (&RTC);
	W24H = w24h;

	display (DISPLAY_MWATCH);
	display (DISPLAY_CWATCH);
}

void watch_set_next () {

	if (++WatchBeingSet > 6)
		WatchBeingSet = 0;
	// The minute watch is always there
	display (DISPLAY_MWATCH_SET);
	// The seconds are only displayed if they are being set; otherwise,
	// it is the calendar
	display (WatchBeingSet ? DISPLAY_CWATCH_SET : DISPLAY_SWATCH_SET);
}

void watch_set_value (sint dir) {

	if (WatchBeingSet == 0)
		watch_ses_button (dir);
	else if (WatchBeingSet < 4)
		watch_mns_button (dir);
	else
		watch_cas_button (dir);
}

// ============================================================================
// Generic status
// ============================================================================

static void gstat_show_common (Boolean blink) {

	stxt (LCD_SEG_L1_2,
		GStat > 1 ? "CLR" : (GStat == 1 ? " ON" : "OFF"), blink);
}

static void gstat_show () { gstat_show_common (NO); }
static void gstats_show () { gstat_show_common (YES); }
static void gstat_clear () { clear_lcd (LCD_SEG_L1_2, LCD_SEG_L1_0); }

// ============================================================================
// Generic numbers
// ============================================================================

static void glnum_show () {

	lword v;
	word seg, chr;

	if (GLNPer)
		ezlcd_item (LCD_SEG_L2_DP, LCD_MODE_SET);

	if ((v = GLNum) > 99999)
		v = 99999;

	for (seg = LCD_SEG_L2_0; seg <= LCD_SEG_L2_4; seg++) {
		if (seg != LCD_SEG_L2_0 && v == 0)
			chr = 64;
		else
			chr = (word)(v % 10);

		ezlcd_item (seg, chr | LCD_MODE_SET);
		v /= 10;
	}
}

static void glnum_clear () { 

	ezlcd_item (LCD_SEG_L2_DP, LCD_MODE_CLEAR);
	clear_lcd (LCD_SEG_L2_0, LCD_SEG_L2_4);
	clear_lcd (LCD_SYMB_AVERAGE, LCD_SYMB_MAX);
}

static void gwnum_show () {

	word v, seg;

	if ((v = GWNum) > 9999)
		v = 9999;

	for (seg = LCD_SEG_L1_0; seg <= LCD_SEG_L2_4; seg++) {

		ezlcd_item (seg,
			((v == 0 && seg != LCD_SEG_L1_0) ? 127 :
				v % 10) | LCD_MODE_SET);
		v /= 10;
	}
}

static void gwnum_clear () { clear_lcd (LCD_SEG_L1_0, LCD_SEG_L2_4); }

// ============================================================================
// Just to show the ACCEL text in the bottom line
// ============================================================================

static void acc_show () { stxt (LCD_SEG_L2_4, "ACCEL", NO); }
static void acc_clear () { clear_lcd (LCD_SEG_L2_4, LCD_SEG_L2_0); }

// ============================================================================
// The radio module
// ============================================================================

static void radio_show () { stxt (LCD_SEG_L2_4, "RADIO", NO); }
static void radio_clear () { clear_lcd (LCD_SEG_L2_4, LCD_SEG_L2_0); }

void radio_display () {

	GStat = RadioOn;
	display (DISPLAY_GSTAT);
	display (DISPLAY_RADIO);
}

Boolean radio_enter_set_mode () {

	display (DISPLAY_GSTAT_SET);
}

void radio_exit_set_mode () {

	display (DISPLAY_GSTAT);
	if (GStat != RadioOn)
		switch_radio (GStat);
}

void radio_set_value (sint dir) {

	if (GStat == 1)
		GStat = 0;
	else
		GStat = 1;
	display (DISPLAY_GSTAT_SET);
}

// ============================================================================
// The accelerator
// ============================================================================

void accel_update_display () {

	GWNum = ((AccelEvents >> 16)) ? 9999 : (word) AccelEvents;

	if (AcsMode == 0)
		GLNum = (lword) AccelLast;
	else if (AcsMode == 1)
		GLNum = (AccelTotal / AccelEvents);
	else
		GLNum = (lword) AccelMax;

	GLNum = (GLNum * 391) / 1000;

	display (DISPLAY_GWNUM);
	display (DISPLAY_GLNUM);
}

void accel_display () {

	AcsMode = 2;		// Show last
	GLNPer = YES;

	accel_option (1);
}

void accel_option (sint dir) {

	if ((dir += AcsMode) > 2)
		AcsMode = 0;
	else if (dir < 0)
		AcsMode = 2;
	else
		AcsMode = (byte)dir;

	clear_lcd (LCD_SYMB_AVERAGE, LCD_SYMB_MAX);
	if (AcsMode)
		ezlcd_item (AcsMode == 1 ? LCD_SYMB_AVERAGE : LCD_SYMB_MAX,
			LCD_MODE_SET);

	accel_update_display ();
}

Boolean accel_enter_set_mode () {

	// Manually, we can only set it on or off overriding the delay
	// parameters
	GStat = AccelOn;
	display (DISPLAY_GSTAT_SET);
	display (DISPLAY_ACCEL);
}

void accel_exit_set_mode () {

	if (GStat != AccelOn) {
		if (GStat > 1)
			accel_clear ();
		accel_start (GStat, 0, 0, 0, 0);
	}
	accel_display ();
}

void accel_set_value (sint dir) {

	if ((dir += GStat) < 0)
		dir = 2;
	else if (dir > 2)
		dir = 0;
	GStat = (byte) dir;
	display (DISPLAY_GSTAT_SET);
}

// ============================================================================
// Pressure/temp
// ============================================================================

static sint LTemp;

static void temp_show () {

	sint w = LTemp;
	word seg;

	if (w < 0) {
		ezlcd_item (LCD_SYMB_ARROW_DOWN, LCD_MODE_SET);
		w = -w;
	} else {
		ezlcd_item (LCD_SYMB_ARROW_DOWN, LCD_MODE_CLEAR);
	}

	for (seg = LCD_SEG_L1_1; seg <= LCD_SEG_L1_3; seg++) {
		ezlcd_item (seg,
			((w == 0 && seg != LCD_SEG_L1_1) ? 127 :
				(w % 10)) | LCD_MODE_SET);
		w /= 10;
	}

	ezlcd_item (LCD_UNIT_L1_DEGREE, LCD_MODE_SET);
	ezlcd_item (LCD_SEG_L1_DP1, LCD_MODE_SET);
	ezlcd_item (LCD_SEG_L1_0, ('C'-'A'+10) | LCD_MODE_SET);
}

static void temp_clear () {

	clear_lcd (LCD_SEG_L1_0, LCD_SEG_L1_3);
	ezlcd_item (LCD_UNIT_L1_DEGREE, LCD_MODE_CLEAR);
	ezlcd_item (LCD_SEG_L1_DP1, LCD_MODE_CLEAR);
}

void press_update_display (lword press, sint temp) {

	LTemp = temp;
	GLNum = (press+5)/10;
	GLNPer = NO;
	display (DISPLAY_TEMP);
	display (DISPLAY_GLNUM);
}

void press_display () {

	press_update_display (0, 0);	// Show zeros to indicate we are on
	press_trigger (0);		// Make sure the sensor thread is up

}

// No option yet (later?), no setting, nothing

// ============================================================================
// ============================================================================
// ============================================================================

static const line_display_t views [N_DISPLAYS] = {

	{ NULL, watch_mn_update, watch_mn_show, watch_mn_clear, 0 },
	{ watch_sc_show, NULL, watch_sc_show, watch_sc_clear, 1 },
	{ NULL, watch_ca_update, watch_ca_show, watch_ca_clear, 1 },
	{ NULL, NULL, watch_mns_show, watch_mns_clear, 0 },
	{ NULL, NULL, watch_ses_show, watch_sc_clear, 1 },	
	{ NULL, NULL, watch_cas_show, watch_ca_clear, 1 },
	{ NULL, NULL, gstat_show, gstat_clear, 0 },
	{ NULL, NULL, gstats_show, gstat_clear, 0 },
	{ NULL, NULL, radio_show, radio_clear, 1 },
	{ NULL, NULL, glnum_show, glnum_clear, 1 },
	{ NULL, NULL, gwnum_show, gwnum_clear, 0 },
	{ NULL, NULL, temp_show, temp_clear, 0 },
	{ NULL, NULL, acc_show, acc_clear, 1 },
};

static const line_display_t	*current [N_WINDOWS];

// ============================================================================

void display_clear (byte where) {

	const line_display_t *win;

	if ((win = current [where]) != NULL) {
		watch_dequeue (win->cb_sec);
		watch_dequeue (win->cb_min);
		if (win->fn_clear)
			win->fn_clear ();
	}
	current [where] = NULL;
}

void display (byte what) {

	const line_display_t *new, *old;
	byte where;

	where = (new = views + what) -> where;

	if ((old = current [where]) != new) {
		// We are changing the display; otherwise it is just a refresh
		display_clear (where);
		watch_queue (new->cb_sec, YES);
		watch_queue (new->cb_min, NO);
		current [where] = new;
	}

	if (new->fn_show)
		new->fn_show ();

}

byte display_content (byte which) {

	byte res;

	return current [which] == NULL ? BNONE :
		(byte) (current [which] - views);
}

void display_rfstat () {

	word seg;

	for (seg = LCD_ICON_RADIO0; seg <= LCD_ICON_RADIO2; seg++)
		ezlcd_item (seg, RadioOn ? LCD_MODE_SET : LCD_MODE_CLEAR);
}
