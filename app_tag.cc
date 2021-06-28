/*
	Copyright 2002-2020 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski & Wlodek Olesinski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "sysio.h"
#include "cc1350.h"
#include "tcvphys.h"
#include "phys_cc1350.h"
#include "plug_null.h"

#include "mpu9250.h"
#include "obmicrophone.h"
#include "bmp280.h"
#include "hdc1000.h"
#include "opt3001.h"

#include "rf.h"

#include "netid.h"
#include "ossi.h"

#define	toggle(a)		((a) = 1 - (a))

#define	RADIO_INITIALLY_ON	2

// ============================================================================
// ============================================================================

#define	MAX_SAMPLES_PER_MINUTE	(256 * 60)
#define	MIN_SAMPLES_PER_MINUTE	1

// This is for adjusting the interval to meet the long-term rate (assuming that
// short-term departures are OK)
#define	MAX_SAMPLE_SPACE	(63 * 1024)
#define	MIN_SAMPLE_SPACE	3

#define	ACTIVATING_BUTTON	0		// Hibernate, radio control
#define	HIBERNATE_ON_PUSH	5		// Seconds to hold the button
#define	SIGNALLING_LEDS		1		// We have just one LED

#include "leddefs.h"

#define	led_bl(n,d)		blink (0, n, d)

#define	WOR_CYCLE		1024			// 1 second
#define	RADIO_LINGER		(5 * 1024)		// 5 seconds
#define	WOR_RSS			20
#define	WOR_PQI			YES

#define	NUMBER_OF_SENSORS	5

// System indexes
#define	SENSOR_MPU9250		1
#define	SENSOR_OBMICROPHONE	2
#define	SENSOR_BMP280		3
#define	SENSOR_HDC1000		4
#define	SENSOR_OPT3001		5

#define	SENSOR_BATTERY		(-1)

// Logical indexes for the application
#define	MPU9250_INDEX		0
#define HDC1000_INDEX		1
#define OBMICROPHONE_INDEX	2
#define OPT3001_INDEX		3
#define BMP280_INDEX		4

#define	MPU9250_FLAG		(1 << MPU9250_INDEX)
#define HDC1000_FLAG		(1 << HDC1000_INDEX)
#define OBMICROPHONE_FLAG	(1 << OBMICROPHONE_INDEX)
#define OPT3001_FLAG		(1 << OPT3001_INDEX)
#define BMP280_FLAG		(1 << BMP280_INDEX)

// ============================================================================

static const word smpl_intervals [] = {
//
// This is shared by all sensors that are sampled in the background
//
	8*1024, 4*1024, 2*1024, 1024, 512, 256, 128, 16
};

// ============================================================================
// MPU9250
// ============================================================================

byte mpu9250_conf [] = { NO, 4, 4, 6, 3, 1, NO };

#define	MPU9250_PAR_MOTION	0	// Offsets
#define	MPU9250_PAR_THRESHOLD	1
#define	MPU9250_PAR_RATE	2
#define	MPU9250_PAR_ACCURACY	3
#define	MPU9250_PAR_BANDWIDTH	4
#define	MPU9250_PAR_COMPONENTS	5
#define	MPU9250_PAR_REPORT	6

static const word mpu9250_rates [] = {
	// 8 selected values: tiny through extreme
	MPU9250_LPA_02,
	MPU9250_LPA_1,
	MPU9250_LPA_2,
	MPU9250_LPA_4,
	MPU9250_LPA_16,
	MPU9250_LPA_64,
	MPU9250_LPA_128,
	MPU9250_LPA_512,
};

static const word mpu9250_thresholds [] = { 0, 8, 16, 24, 32, 48, 64, 128 };

static const word mpu9250_bandwidth [] = {
	MPU9250_LPF_5,
	MPU9250_LPF_10,
	MPU9250_LPF_20,
	MPU9250_LPF_42,
	MPU9250_LPF_98,
	MPU9250_LPF_188,
	MPU9250_LPF_256,
	MPU9250_LPF_2100,
};

static const word mpu9250_accuracy [] = {
	MPU9250_ACCEL_RANGE_16 | MPU9250_GYRO_RANGE_2000,	// tiny
	MPU9250_ACCEL_RANGE_16 | MPU9250_GYRO_RANGE_2000,	// low
	MPU9250_ACCEL_RANGE_8  | MPU9250_GYRO_RANGE_1000,	// small
	MPU9250_ACCEL_RANGE_8  | MPU9250_GYRO_RANGE_1000,	// medium
	MPU9250_ACCEL_RANGE_4  | MPU9250_GYRO_RANGE_500,	// big
	MPU9250_ACCEL_RANGE_4  | MPU9250_GYRO_RANGE_500,	// high
	MPU9250_ACCEL_RANGE_2  | MPU9250_GYRO_RANGE_250,	// huge
	MPU9250_ACCEL_RANGE_2  | MPU9250_GYRO_RANGE_250,	// extreme
};

static const byte mpu9250_desc_length [] = {
	// Based on component selection: AGCT
	0, 6, 6, 12, 6, 12, 12, 18, 2, 8, 8, 14, 8, 14, 14, 20
};

typedef struct {
	byte motion, components;
	// Only needed in motion mode
	word values [4];
} mpu9250_desc_t;

static mpu9250_desc_t mpu9250_desc;

// ============================================================================
// HDC1000
// ============================================================================

byte hdc1000_conf [] = { NO, 7, 1, 4 };	// Sampled in BGR

#define	HDC1000_PAR_HEATER	0
#define	HDC1000_PAR_ACCURACY	1
#define	HDC1000_PAR_COMPONENTS	2

static const word hdc1000_accuracy [] = {
	HDC1000_MODE_TR11 | HDC1000_MODE_HR8,
	HDC1000_MODE_TR11 | HDC1000_MODE_HR11,
	HDC1000_MODE_TR14 | HDC1000_MODE_HR11,
	HDC1000_MODE_TR11 | HDC1000_MODE_HR14,
	HDC1000_MODE_TR14 | HDC1000_MODE_HR14,
	HDC1000_MODE_TR14 | HDC1000_MODE_HR14,
	HDC1000_MODE_TR14 | HDC1000_MODE_HR14,
	HDC1000_MODE_TR14 | HDC1000_MODE_HR14,
};

typedef struct {
	byte components;
	word smplint;	// Sampling interval
	word values [2];
} hdc1000_desc_t;

static hdc1000_desc_t hdc1000_desc;

// ============================================================================
// OBMICROPHONE
// ============================================================================

byte obmicrophone_conf [1] = { 4 };

static const word obmicrophone_rates [] = {
	1000,
	4000,
	8000,
	12000,
	15000,
	18000,
	22000,
	24750,
};

// ============================================================================
// OPT3001
// ============================================================================

byte opt3001_conf [] = { NO, 3, 4 };		// Sampled in BGR

static const word opt3001_accuracy [] = {
	OPT3001_MODE_TIME_100,
	OPT3001_MODE_TIME_100,
	OPT3001_MODE_TIME_100,
	OPT3001_MODE_TIME_100,
	OPT3001_MODE_TIME_800,
	OPT3001_MODE_TIME_800,
	OPT3001_MODE_TIME_800,
	OPT3001_MODE_TIME_800,
};

typedef struct {

	word smplint;	// Sampling interval
	word values [2];

} opt3001_desc_t;

static opt3001_desc_t opt3001_desc;

// ============================================================================
// BMP280
// ============================================================================

byte bmp280_conf [] = { YES, 7, 4, 7, 1, 4 };	// Sampled in BGR

static const word bmp280_accuracy [] = {
	BMP280_MODE_PRESS_OVS_1X  | BMP280_MODE_TEMP_OVS_1X,
	BMP280_MODE_PRESS_OVS_1X  | BMP280_MODE_TEMP_OVS_1X,
	BMP280_MODE_PRESS_OVS_2X  | BMP280_MODE_TEMP_OVS_2X,
	BMP280_MODE_PRESS_OVS_2X  | BMP280_MODE_TEMP_OVS_2X,
	BMP280_MODE_PRESS_OVS_4X  | BMP280_MODE_TEMP_OVS_4X,
	BMP280_MODE_PRESS_OVS_4X  | BMP280_MODE_TEMP_OVS_4X,
	BMP280_MODE_PRESS_OVS_8X  | BMP280_MODE_TEMP_OVS_8X,
	BMP280_MODE_PRESS_OVS_16X | BMP280_MODE_TEMP_OVS_16X,
};

static const word bmp280_rates [] = {
	BMP280_MODE_STANDBY_4000_MS,
	BMP280_MODE_STANDBY_2000_MS,
	BMP280_MODE_STANDBY_1000_MS,
	BMP280_MODE_STANDBY_500_MS,
	BMP280_MODE_STANDBY_250_MS,
	BMP280_MODE_STANDBY_125_MS,
	BMP280_MODE_STANDBY_63_MS,
	BMP280_MODE_STANDBY_05_MS,
};
				
static const word bmp280_bandwidth [] = {
	BMP280_MODE_FILTER_16,
	BMP280_MODE_FILTER_16,
	BMP280_MODE_FILTER_8,
	BMP280_MODE_FILTER_8,
	BMP280_MODE_FILTER_4,
	BMP280_MODE_FILTER_4,
	BMP280_MODE_FILTER_2,
	BMP280_MODE_FILTER_OFF,
};

typedef struct {

	byte components;
	word smplint;	// Sampling interval
	lword values [2];

} bmp280_desc_t;

static bmp280_desc_t bmp280_desc;

// ============================================================================
// ============================================================================
	
static cc1350_rfparams_t	RFP = {
					WOR_CYCLE,
					RADIO_LINGER,
					WOR_RSS,
					WOR_PQI
				};

static byte	RadioOn,		// 0, 1 [WOR], 2 [full ON]
		Sensors,		// Sensor status
		LastRef;

static word	SamplesPerMinute,	// Target rate
		SampleSpace;		// Adjustable space to meet target rate

static lword	SetTime,		// Time offset
		SampleStartSecond,	// When sampling started
		SamplesTaken,		// Samples taken so far
		SamplesToTake;		// Remaining samples to send

static sint	RFC;

// ============================================================================

static void switch_radio (byte on) {

	word par;

	if (RadioOn != on) {

		RadioOn = on;
		if (RadioOn > 1) {
			// Full on
			tcv_control (RFC, PHYSOPT_RXON, NULL);
			return;
		}

		par = RadioOn;
		tcv_control (RFC, PHYSOPT_OFF, &par);
	}
}

static void toggle_radio () {

	byte what;

	if ((what = RadioOn + 1) > 2)
		what = 0;

	switch_radio (what);

	led_bl (2 + 2 * what, 128);
}

// ============================================================================

static address new_msg (byte code, word len) {
//
// Tries to acquire a packet for outgoing RF message; len == parameter length
//
	address msg;

	if (len & 1)
		len++;
	if ((msg = tcv_wnp (WNONE, RFC, len + RFPFRAME + sizeof (oss_hdr_t))) !=
	    NULL) {
		msg [1] = NODE_ID;
		osshdr (msg) -> code = code;
		osshdr (msg) -> ref = LastRef;
	}
	return msg;
}

static void oss_ack (word status) {

	address msg;

	led_bl ((status ? 2 : 1), 64);

	if ((msg = new_msg (0, sizeof (status))) != NULL) {
		osspar (msg) [0] = status;
		tcv_endp (msg);
	}
}

// ============================================================================

static word scaled_option (const word *t, byte v) {
//
// To play it safe
//
	if (v > 7)
		v = 7;
	return t [v];
}

fsm mpu9250_sampler {
//
// This is used for detecting motion events; otherwise, the sensor is sampled
// at the moment of report, as the only sensor with this property
//
	state MP_MOTION:

		address msg;
		message_report_t *pmt;

		read_sensor (MP_MOTION, SENSOR_MPU9250, mpu9250_desc.values);
		// The number of motion events
		mpu9250_desc.values [3] ++;

		if (mpu9250_desc.motion > 1 &&
		    (msg = new_msg (message_report_code,
			sizeof (message_report_t) + 8)) != NULL) {
			// Send a report message, just skip if memory problems
			pmt = (message_report_t*) osspar (msg);
			// IMU motion report
			pmt->layout = 0x10;
			pmt->data.size = 8;
			memcpy (pmt->data.content, &mpu9250_desc.values, 8);
			tcv_endp (msg);
		}
			
	initial state MP_LOOP:

		wait_sensor (SENSOR_MPU9250, MP_MOTION);
		release;
}

static void sensor_on_mpu9250 () {

	word options;

	if (Sensors & MPU9250_FLAG)
		// The sensor is on, do nothing
		return;

	options = scaled_option (mpu9250_rates, mpu9250_conf [2]) |
		  scaled_option (mpu9250_accuracy, mpu9250_conf [3]) |
		  scaled_option (mpu9250_bandwidth, mpu9250_conf [4]);

	if (mpu9250_conf [0]) {
		options |= MPU9250_LP_MOTION_DETECT;
		// Force no components to indicate the there is a motion count
		// report on output
		mpu9250_conf [5] = 0;
	} else if (mpu9250_conf [5] == 0) {
		// Make sure there is at least one component, if no motion
		mpu9250_conf [5] = 1;
	}

	// Motion implies ACCEL only, single value returned

	if ((mpu9250_conf [5] & 1) || mpu9250_conf [0])
		options |= MPU9250_SEN_ACCEL;
	if (mpu9250_conf [5] & 2)
		options |= MPU9250_SEN_GYRO;
	if (mpu9250_conf [5] & 4)
		options |= MPU9250_SEN_COMPASS;
	if (mpu9250_conf [5] & 4)
		options |= MPU9250_SEN_TEMP;

	mpu9250_on (options,
		scaled_option (mpu9250_thresholds, mpu9250_conf [1]));

	bzero (&mpu9250_desc, sizeof (mpu9250_desc));

	if (mpu9250_conf [0]) {
		mpu9250_desc . motion = 1;
		if (mpu9250_conf [6])
			// Event reports on motion
			mpu9250_desc . motion |= 2;
		// Start the sampler to collect (and optionally report) motion
		// events
		if (!running (mpu9250_sampler))
			runfsm mpu9250_sampler;
	}

	mpu9250_desc . components = mpu9250_conf [5];

	_BIS (Sensors, MPU9250_FLAG);
}

static void sensor_off_mpu9250 () {

	if ((Sensors & MPU9250_FLAG) == 0)
		return;

	mpu9250_off ();

	killall (mpu9250_sampler);

	_BIC (Sensors, MPU9250_FLAG);
}

fsm hdc1000_sampler {

	state HD_LOOP:

		read_sensor (HD_LOOP, SENSOR_HDC1000, hdc1000_desc.values);
		delay (hdc1000_desc.smplint, HD_LOOP);
}

static void sensor_on_hdc1000 () {

	word options;

	if (Sensors & HDC1000_FLAG)
		return;

	options = scaled_option (hdc1000_accuracy, hdc1000_conf [1]);
	if (hdc1000_conf [0])
		options |= HDC1000_MODE_HEATER;
	if (hdc1000_conf [2] == 0)
		hdc1000_conf [2] = 1;
	if (hdc1000_conf [2] & 1)
		options |= HDC1000_MODE_HUMID;
	if (hdc1000_conf [2] & 2)
		options |= HDC1000_MODE_TEMP;

	bzero (&hdc1000_desc, sizeof (hdc1000_desc));
	hdc1000_desc.components = hdc1000_conf [2];
	hdc1000_desc.smplint = scaled_option (smpl_intervals,
		hdc1000_conf [3]);

	hdc1000_on (options);

	if (!running (hdc1000_sampler))
		runfsm hdc1000_sampler;

	_BIS (Sensors, HDC1000_FLAG);
}

static void sensor_off_hdc1000 () {

	if ((Sensors & HDC1000_FLAG) == 0)
		return;

	hdc1000_off ();

	killall (hdc1000_sampler);

	_BIC (Sensors, HDC1000_FLAG);
}

static void sensor_on_obmicrophone () {

	if (Sensors & OBMICROPHONE_FLAG)
		return;

	obmicrophone_on (scaled_option (obmicrophone_rates,
		obmicrophone_conf [0]));

	// This one is not sampled in the background

	_BIS (Sensors, OBMICROPHONE_FLAG);
}

static void sensor_off_obmicrophone () {

	if ((Sensors & OBMICROPHONE_FLAG) == 0)
		return;

	obmicrophone_off ();

	_BIC (Sensors, OBMICROPHONE_FLAG);
}

fsm opt3001_sampler {

	state OP_LOOP:

		read_sensor (OP_LOOP, SENSOR_OPT3001, opt3001_desc.values);
		delay (opt3001_desc.smplint, OP_LOOP);
}

static void sensor_on_opt3001 () {

	if (Sensors & OPT3001_FLAG)
		return;

	opt3001_on (scaled_option (opt3001_accuracy, opt3001_conf [1]) |
		OPT3001_MODE_AUTORANGE | (opt3001_conf [0] ?
			OPT3001_MODE_CMODE_CN : OPT3001_MODE_CMODE_SS));

	// For now, we only use autorange

	bzero (&opt3001_desc, sizeof (opt3001_desc));
	opt3001_desc.smplint = scaled_option (smpl_intervals,
		opt3001_conf [2]);

	if (!running (opt3001_sampler))
		runfsm opt3001_sampler;

	_BIS (Sensors, OPT3001_FLAG);
}

static void sensor_off_opt3001 () {

	if ((Sensors & OPT3001_FLAG) == 0)
		return;

	opt3001_off ();

	killall (opt3001_sampler);

	_BIC (Sensors, OPT3001_FLAG);
}

fsm bmp280_sampler {

	state BM_LOOP:

		read_sensor (BM_LOOP, SENSOR_BMP280,
			(address)(bmp280_desc.values));
		delay (bmp280_desc.smplint, BM_LOOP);
}

static void sensor_on_bmp280 () {

	word options;

	if (Sensors & BMP280_FLAG)
		return;

	options = scaled_option (bmp280_rates, bmp280_conf [1]) |
		  scaled_option (bmp280_accuracy, bmp280_conf [2]) |
		  scaled_option (bmp280_bandwidth, bmp280_conf [3]) |
		  (bmp280_conf [0] ? BMP280_MODE_FORCED : BMP280_MODE_NORMAL);

	if (bmp280_conf [4] == 0)
		bmp280_conf [4] = 1;

	if ((bmp280_conf [4] & 1) == 0)
		// Mask out the pressure
		options &= ~BMP280_MODE_PRESS_OVS_MASK;

	if ((bmp280_conf [4] & 2) == 0)
		// Mask out the temp
		options &= ~BMP280_MODE_TEMP_OVS_MASK;

	bzero (&bmp280_desc, sizeof (bmp280_desc));
	bmp280_desc.components = bmp280_conf [4];
	bmp280_desc.smplint = scaled_option (smpl_intervals, bmp280_conf [5]);

	bmp280_on (options);

	if (!running (bmp280_sampler))
		runfsm bmp280_sampler;

	_BIS (Sensors, BMP280_FLAG);
}

static void sensor_off_bmp280 () {

	if ((Sensors & BMP280_FLAG) == 0)
		return;

	bmp280_off ();

	killall (bmp280_sampler);

	_BIC (Sensors, BMP280_FLAG);
}

// ============================================================================

static word sensor_config_one (byte *opt, sint nopt, byte *pmt, word pml) {

	sint par, val;

	while (pml--) {
		par = (*pmt >> 4) & 0x0f;
		val = (*pmt & 0x0f);
		if (par >= nopt)
			return ACK_PARAM;
		opt [par] = (byte) val;
		pmt++;
	}

	return ACK_OK;
}
		
static word sensor_config (address par, word pml) {
//
// Configure sensors
//
	sint len, lft;
	word sen;
	byte *buf;

#define	pmt	((command_config_t*)par)

	if (pml < (lft = pmt->confdata.size) + 2)
		return ACK_LENGTH;

	buf = pmt->confdata.content;

	while (lft) {
		// Sensor number
		sen = (*buf >> 4) & 0x0f;
		// Length of the chunk
		len = (*buf & 0x0f) + 1;
		buf++;
		lft--;
		if (len > lft)
			return ACK_LENGTH;

		switch (sen) {
			case MPU9250_INDEX:
				sen = sensor_config_one (mpu9250_conf,
					sizeof (mpu9250_conf), buf, len);
				break;
			case HDC1000_INDEX:
				sen = sensor_config_one (hdc1000_conf,
					sizeof (hdc1000_conf), buf, len);
				break;
			case OBMICROPHONE_INDEX:
				sen = sensor_config_one (obmicrophone_conf,
					sizeof (obmicrophone_conf), buf, len);
				break;
			case OPT3001_INDEX:
				sen = sensor_config_one (opt3001_conf,
					sizeof (opt3001_conf), buf, len);
				break;
			case BMP280_INDEX:
				sen = sensor_config_one (bmp280_conf,
					sizeof (bmp280_conf), buf, len);
				break;

			default:
				return ACK_PARAM;
		}

		if (sen)
			return sen;

		lft -= len;
		buf += len;
	}

#undef	pmt

	return ACK_OK;
}

static void (*sen_turn_fun [2][NUMBER_OF_SENSORS])() = { 
	 { sensor_on_mpu9250, sensor_on_hdc1000, sensor_on_obmicrophone,
		sensor_on_opt3001, sensor_on_bmp280 },
	 { sensor_off_mpu9250, sensor_off_hdc1000, sensor_off_obmicrophone,
		sensor_off_opt3001, sensor_off_bmp280 }
};

static word sensor_onoff (byte s) {
//
// Turn sensors on or off
//
	byte sns;
	sint b, i;

	// Select the on or off function set: 0x80 == ON, 0x00 == OFF
	b = (s & 0x80) ? 0 : 1;
	sns = s & 0x1f;

	if (sns == 0x00)
		// Select ALL
		sns = 0x1f;

	for (i = 0; i < NUMBER_OF_SENSORS; i++)
		if (sns & (1 << i))
			sen_turn_fun [b][i] ();
	return ACK_OK;
}

#define	all_sensors_off()	sensor_onoff (0x00)

fsm delayed_switch (byte opn) {

	state DS_START:

		if (opn == 0 || opn == 3) {
			// Radio goes off or hibernate; give it one second for
			// the ACK to get through and then proceed
			if (opn)
				led_bl (64, 72);
			else
				led_bl (16, 150);
			delay (1024, DS_SWITCH);
			release;
		}

		// Normal radio switch

	state DS_RADIO:

		switch_radio (opn);
		finish;

	state DS_SWITCH:

		if (opn < 3)
			sameas DS_RADIO;

		// Hibernate
		switch_radio (0);
		all_sensors_off ();

		// A bit more delay, so the LEDs finish blinking
		delay (64 * 72, DS_HIBERNATE);
		release;

	state DS_HIBERNATE:

		leds_off;
		hibernate ();
}

static word radio_command (address par, word pml) {

	word val;
	byte mod, ope;

	if (pml < 5)
		return ACK_LENGTH;

#define	pmt	((command_radio_t*)par)

	mod = NO;

	if (pmt->options > 3)
		return ACK_PARAM;

	if ((val = pmt->worintvl) != 0) {
		// Keep it sane
		if (val < 256)
			val = 256;
		else if (val > 8192)
			val = 8192;
		if (val != RFP.interval) {
			mod = YES;
			RFP.interval = val;
		}
	}

	if ((val = pmt->offdelay) != 0) {
		if (val < 20)
			val = 20;
		if (val != RFP.offdelay) {
			mod = YES;
			RFP.interval = val;
		}
	}

	if (mod)
		tcv_control (RFC, PHYSOPT_SETPARAMS, (address)&RFP);

	runfsm delayed_switch (pmt->options);

	return ACK_OK;

#undef	pmt
}

static word sensor_status (byte which, byte *where) {
//
// Return the status of the indicated sensors in where
//
	sint i, j, nb, nn;
	word sl;
	byte *st;

	// Start from a filled dummy byte number -1
	nb = -1;
	nn = 1;

#define	setting(a)	st = (a); sl = sizeof (a); break;

	for (i = 0; i < NUMBER_OF_SENSORS; i++) {
		if ((which & (1 << i)) == 0)
			continue;
		switch (i) {
			case MPU9250_INDEX: 	   setting (mpu9250_conf);
			case HDC1000_INDEX: 	   setting (hdc1000_conf);
			case OBMICROPHONE_INDEX:   setting (obmicrophone_conf);
			case OPT3001_INDEX: 	   setting (opt3001_conf);
			case BMP280_INDEX: 	   setting (bmp280_conf);
		}
		for (j = 0; j < sl; j++) {
			toggle (nn);
			if (nn) {
				// Second nibble
				if (where)
					where [nb] |= (st [j] & 0x0f);
			} else {
				// First nibble in a new byte
				nb++;
				if (where) 
					where [nb] = (st [j] << 4);
			}
		}
	}
#undef	setting

	// nb is the last byte filled, return the length in bytes
	return nb + 1;
}

fsm status_sender {

	state WAIT_BATTERY:

		word batt, blen;
		address msg;
		message_status_t *pmt;

		read_sensor (WAIT_BATTERY, SENSOR_BATTERY, &batt);

		// Determine the blob length
		blen = sensor_status (0xff, NULL);

		if ((msg = new_msg (message_status_code,
		    sizeof (message_status_t) + blen)) == NULL) {
			// Keep waiting for memory, this will not happen
			delay (256, WAIT_BATTERY);
			release;
		}

		pmt = (message_status_t*) osspar (msg);
		pmt->uptime = seconds ();
		pmt->seconds = pmt->uptime + SetTime;
		pmt->left = SamplesToTake - SamplesTaken;
		pmt->battery = batt;
		pmt->freemem = memfree (0, &(pmt->minmem));
		pmt->rate = SamplesPerMinute;
		pmt->sset = Sensors;

		pmt->sstat.size = sensor_status (0xff,
			(byte*)(((word*)&(pmt->sstat.size)) + 1));

		tcv_endp (msg);
		finish;
}

static word sensor_report (byte which, byte *where, address mask) {
//
// Returns the sensor report
//
	word nb, cb;

	nb = 0;

	if (where)
		// Not a dry run
		*mask = 0;

	if (which & MPU9250_FLAG) {
		// The IMU is on
		if (mpu9250_desc.components & 0x10) {
			// Motion report only
			nb += 8;
			if (where) {
				memcpy (where, mpu9250_desc.values, 8);
				// Zero out motion count
				mpu9250_desc.values [3] = 0;
				where += 8;
				*mask |= 0x10;
			}
		} else {
			nb += (cb =
			    mpu9250_desc_length [mpu9250_desc.components]);
			if (where) {
				read_sensor (WNONE, SENSOR_MPU9250,
					(address) where);
				where += cb;
				*mask |= mpu9250_desc.components;
			}
		}
	}

	if (which & HDC1000_FLAG) {
		nb += (cb = (hdc1000_desc.components == 3) ? 4 : 2);
		if (where) {
			memcpy (where, hdc1000_desc.values, cb);
			where += cb;
			*mask |= (hdc1000_desc.components << 5);
		}
	}

	if (which & OBMICROPHONE_FLAG) {
		nb += 8;
		if (where) {
			read_sensor (WNONE, SENSOR_OBMICROPHONE,
			    (address) where);
			obmicrophone_reset ();
			where += 8;
			*mask |= 1 << 7;
		}
	}

	if (which & OPT3001_FLAG) {
		nb += 2;
		// We only return the first word
		if (where) {
			*((address)where) = opt3001_desc.values [0];
			where += 2;
			*mask |= 1 << 8;
		}
	}

	if (which & BMP280_FLAG) {
		nb += (cb = (bmp280_desc.components == 3) ? 8 : 4);
		if (where) {
			memcpy (where, bmp280_desc.values, cb);
			where += cb;
			*mask |= (bmp280_desc.components << 9);
		}
	}

	return nb;
}

fsm sampler {
//
// These sensors are sampled in the background: HDC1000, OPT3001, BMP280
//
	state SM_TAKE:

		address msg;
		message_report_t *pmt;
		word bl;

		// Calculate the report size
		bl = sensor_report (Sensors, NULL, NULL);

		if ((msg = new_msg (message_report_code,
			sizeof (message_report_t) + bl)) == NULL) {
			// Failure, do we skip?
			if (SampleSpace > 128) {
				// Some heuristics
				delay (16, SM_TAKE);
				release;
			}
			// Just skip
			sameas SM_DELAY;
		}

		// Fill in the message
		pmt = (message_report_t*) osspar (msg);
		pmt->sample = (word) SamplesTaken;
		pmt->data.size = sensor_report (Sensors,
			(byte*)(pmt->data.content), &(pmt->layout));

		tcv_endp (msg);

		SamplesTaken++;
		
	initial state SM_DELAY:

		lword ns, nm;

		if (SamplesTaken >= SamplesToTake) {
			SamplesTaken = SamplesToTake = 0;
			finish;
		}

		delay (SampleSpace, SM_TAKE);
}

fsm sample_time_corrector {

	lword NextMinuteBoundary;

	state STC_START:

		NextMinuteBoundary = seconds () + 60;

	state STC_WAIT_A_MINUTE:

		lword s;

		// Wait until hit the next minute boundary
		if ((s = seconds ()) < NextMinuteBoundary) {
			delay ((word)((NextMinuteBoundary - s) * 1023),
				STC_WAIT_A_MINUTE);
			release;
		}

		s = (SampleSpace *
			(((NextMinuteBoundary - SampleStartSecond) / 60) *
				SamplesPerMinute)) / SamplesTaken;

		SampleSpace = s > MAX_SAMPLE_SPACE ? MAX_SAMPLE_SPACE :
			(s < MIN_SAMPLE_SPACE ? MIN_SAMPLE_SPACE : (word) s);

		NextMinuteBoundary += 60;

		delay (999, STC_WAIT_A_MINUTE);
}

static void stop_sampling () {

	killall (sampler);
	killall (sample_time_corrector);
	SamplesToTake = SamplesTaken = SampleStartSecond = 0;
	SamplesPerMinute = 0;
}

static word sample_command (address par, word pml) {

	if (pml < 10)
		return ACK_LENGTH;

#define	pmt	((command_sample_t*)par)

	if (pmt->spm == 0)
		// This is the default: one sample per second, 1 sample per
		// minute is OK
		pmt->spm = 60;
	else if (pmt->spm > MAX_SAMPLES_PER_MINUTE)
		pmt->spm = MAX_SAMPLES_PER_MINUTE;

	if (pmt->seconds)
		// Set the time
		SetTime = pmt->seconds - seconds ();

	SamplesPerMinute = pmt->spm;
	// Calculate a rough estimate of the inter-sample interval; we will be
	// adjusting it to try to keep the target rate in samples per minute
	SampleSpace = (60 * 1024) / pmt->spm;
	SamplesToTake = pmt->nsamples;
	SamplesTaken = 0;
	SampleStartSecond = seconds ();

	// Reset things
	killall (sampler);
	killall (sample_time_corrector);
	if (runfsm sampler) {
		if (runfsm sample_time_corrector)
			return ACK_OK;
	}

	stop_sampling ();
	return ACK_NORES;
#undef	pmt
}

static word stop_command (address par, word pml) {

	if (!running (sampler))
		return ACK_VOID;

	stop_sampling ();

	return ACK_OK;
}

static void handle_rf_command (byte code, address par, word pml) {

	word ret;

	switch (code) {

		case command_config_code:

			// Configure sensors
			ret = sensor_config (par, pml);
			break;

		case command_onoff_code:

			ret = sensor_onoff (*((byte*)par));
			break;

		case command_radio_code:

			ret = radio_command (par, pml);
			break;

		case command_status_code:

			// Respond with status
			if (running (status_sender))
				oss_ack (ACK_BUSY);
			else if (!runfsm status_sender)
				oss_ack (ACK_NORES);
			return;

		case command_sample_code:

			// Start sampling
			ret = sample_command (par, pml);
			break;

		case command_stop_code:

			ret = stop_command (par, pml);
			break;
	
		default:

			ret = ACK_COMMAND;
	}
			
	oss_ack (ret);
}

fsm radio_receiver {

	state RS_LOOP:

		address pkt;
		oss_hdr_t *osh;

		pkt = tcv_rnp (RS_LOOP, RFC);

		if (tcv_left (pkt) >= OSSMINPL && pkt [1] == NODE_ID &&
		    (osh = osshdr (pkt)) -> ref != LastRef) {

			LastRef = osh -> ref;

			handle_rf_command (
				osh->code,
				osspar (pkt),
				tcv_left (pkt) - OSSFRAME
			);
		}

		tcv_endp (pkt);
		sameas RS_LOOP;
}

// ============================================================================

fsm button_holder {

	word counter;

	state BH_START:

		counter = 1024 * HIBERNATE_ON_PUSH;

	state BH_LOOP:

		delay (1, BH_TRY);
		release;

	state BH_TRY:

		if (!button_down (ACTIVATING_BUTTON)) {
			// A short push. toggle radio
			toggle_radio ();
			finish;
		}

		if (counter) {
			counter--;
			sameas BH_LOOP;
		}

	state BH_HIBERNATE:

		if (!runfsm delayed_switch (3))
			sameas BH_LOOP;

		finish;
}

static void buttons (word but) {

	switch (but) {

		case ACTIVATING_BUTTON:
			// Run a thread to detect a long push, to power down
			// the device
			if (running (button_holder))
				// Ignore
				return;

			runfsm button_holder;
			return;
		// One more button left for grabs
	}

	// Tacitly ignore other buttons
}
		
fsm root {

	word depcnt;

	state RS_INIT:

		word sid;

		powerdown ();

		phys_cc1350 (0, MAX_PACKET_LENGTH);
		tcv_plug (0, &plug_null);

		RFC = tcv_open (NONE, 0, 0);
		sid = NETID;
		tcv_control (RFC, PHYSOPT_SETSID, &sid);
		tcv_control (RFC, PHYSOPT_SETPARAMS, (address)&RFP);

		runfsm radio_receiver;

#ifdef RADIO_INITIALLY_ON
		switch_radio (RADIO_INITIALLY_ON);
#endif
		buttons_action (buttons);

		// That's it, no more use for us
		finish;
}

// ============================================================================
// ============================================================================

// Notes:
//	- add "keep receiver open when waiting for IDLE" in RF
