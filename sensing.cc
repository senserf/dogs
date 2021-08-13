/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "tag.h"
#include "sensing.h"
#include "sampling.h"
#include "streaming.h"
#include "ossint.h"

static const word smpl_intervals [] = {
//
// This is shared by all sensors that are sampled in the background
//
	8*1024, 4*1024, 2*1024, 1024, 512, 256, 128, 16
};

// ============================================================================
// MPU9250
// ============================================================================

static const word mpu9250_lprates [] = {
	MPU9250_LPA_02,
	MPU9250_LPA_05,
	MPU9250_LPA_1,
	MPU9250_LPA_2,
	MPU9250_LPA_4,
	MPU9250_LPA_8,
	MPU9250_LPA_16,
	MPU9250_LPA_32,
	MPU9250_LPA_64,
	MPU9250_LPA_128,
	MPU9250_LPA_256,
	MPU9250_LPA_512,
};

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

static const word mpu9250_ranges [] = {
	MPU9250_ACCEL_RANGE_2  | MPU9250_GYRO_RANGE_250,
	MPU9250_ACCEL_RANGE_4  | MPU9250_GYRO_RANGE_500,
	MPU9250_ACCEL_RANGE_8  | MPU9250_GYRO_RANGE_1000,
	MPU9250_ACCEL_RANGE_16 | MPU9250_GYRO_RANGE_2000
};

static const byte mpu9250_desc_length [] = {
	// Based on component selection: AGCT; actually TCGA treated as bits
	// in a value 0-15, so A = 1, CG = 6, and so on:
	// .... = 0, ...A = 6, ..G. = 6, ..GA = 12, ...
	0, 6, 6, 12, 6, 12, 12, 18, 2, 8, 8, 14, 8, 14, 14, 20, 2
};

// 0 event: 0, 1, 2 (none, motion, dataready)
// 1 threshold				
// 2 rate
// 3 accuracy
// 4 bandwidth
// 5 components
// ============
// 6 report
// 7 datarate

static word mpu9250_conf [] =       { 0, 32, 6, 0, 3, 7, 1 };
static const byte mpu9250_clen [] = { 0,  0, 0, 0, 0, 0, 0 };

mpu9250_desc_t mpu9250_desc;

// ============================================================================
// HDC1000
// ============================================================================

static const word hdc1000_accuracy [] = {
	HDC1000_MODE_TR11 | HDC1000_MODE_HR8,
	HDC1000_MODE_TR11 | HDC1000_MODE_HR11,
	HDC1000_MODE_TR14 | HDC1000_MODE_HR11,
	HDC1000_MODE_TR14 | HDC1000_MODE_HR14
};

static word hdc1000_conf [] =       { 0, 1, 4096, 1 };		// Sampled
static const byte hdc1000_clen [] = { 0, 0,    1, 0 };

static hdc1000_desc_t hdc1000_desc;

// ============================================================================
// OBMICROPHONE
// ============================================================================

static word obmicrophone_conf [] =       { 1500 };		// 1.5 MHz
static const byte obmicrophone_clen [] = {    1 };

// ============================================================================
// OPT3001
// ============================================================================

static const word opt3001_accuracy [] = {
	OPT3001_MODE_TIME_100,
	OPT3001_MODE_TIME_800
};

static word opt3001_conf [] =       { 0, 0, 4096 };		// Sampled
static const byte opt3001_clen [] = { 0, 0,    1 };

static opt3001_desc_t opt3001_desc;

// ============================================================================
// BMP280
// ============================================================================

static const word bmp280_accuracy [] = {
	BMP280_MODE_PRESS_OVS_1X  | BMP280_MODE_TEMP_OVS_1X,
	BMP280_MODE_PRESS_OVS_2X  | BMP280_MODE_TEMP_OVS_2X,
	BMP280_MODE_PRESS_OVS_4X  | BMP280_MODE_TEMP_OVS_4X,
	BMP280_MODE_PRESS_OVS_8X  | BMP280_MODE_TEMP_OVS_8X,
	BMP280_MODE_PRESS_OVS_16X | BMP280_MODE_TEMP_OVS_16X
};

static const word bmp280_rates [] = {
	BMP280_MODE_STANDBY_4000_MS,
	BMP280_MODE_STANDBY_2000_MS,
	BMP280_MODE_STANDBY_1000_MS,
	BMP280_MODE_STANDBY_500_MS,
	BMP280_MODE_STANDBY_250_MS,
	BMP280_MODE_STANDBY_125_MS,
	BMP280_MODE_STANDBY_63_MS,
	BMP280_MODE_STANDBY_05_MS
};
				
static const word bmp280_bandwidth [] = {
	BMP280_MODE_FILTER_16,
	BMP280_MODE_FILTER_8,
	BMP280_MODE_FILTER_4,
	BMP280_MODE_FILTER_2,
	BMP280_MODE_FILTER_OFF
};

static word bmp280_conf [] =       { 0, 0, 2, 2, 4096, 1 };	// Sampled
static const byte bmp280_clen [] = { 0, 0, 0, 0,    1, 0 };

static bmp280_desc_t bmp280_desc;

// Active sensors
byte Sensors;

// ============================================================================

#define	scaled_option(t,v)	(((v) >= sizeof (t)) ? t [sizeof (t) - 1] : \
					t [v])
						
// ============================================================================

fsm mpu9250_sampler {
//
// This is used for detecting motion events; otherwise, the sensor is sampled
// at the moment of report. The FSM can only be running if mpu9250_desc.evtype
// is 1.
//
	state MP_MOTION:

		word values [3];

		read_mpu9250 (MP_MOTION, values);

		// The number of motion events
		mpu9250_desc.motion_events ++;
		if (mpu9250_desc.evtype & 0x80)
			ossint_motion_event (values,
				mpu9250_desc.motion_events);

	initial state MP_LOOP:

		wait_sensor (SENSOR_MPU9250, MP_MOTION);
		release;
}

static void sensor_on_mpu9250 () {

	lword options;

	if (mpu9250_active)
		// The sensor is on, do nothing
		return;

	bzero (&mpu9250_desc, sizeof (mpu9250_desc));

	options = scaled_option (mpu9250_lprates, mpu9250_conf [2]) |
		  scaled_option (mpu9250_ranges, mpu9250_conf [3]) |
		  scaled_option (mpu9250_bandwidth, mpu9250_conf [4]);

	// Sanity
	if (mpu9250_conf [6] > 0xf)
		mpu9250_conf [5] = 0;

	// Make sense of the event option
	if ((mpu9250_conf [0] & 0x04)) {
		// Motion detect, event type 1
		mpu9250_desc.evtype = 1;
		options |= MPU9250_LP_MOTION_DETECT | MPU9250_SEN_ACCEL;
	} else if (mpu9250_conf [0] & 0x02) {
		// Sync data read, event type 2
		mpu9250_desc.evtype = 2;
		options |= MPU9250_SYNC_READ;
	} else {
		// None, no events
		mpu9250_desc.evtype = 0;
	}

	if (mpu9250_conf [6] == 0)
		// Make sure there is at least one component, i.e., accel
		mpu9250_conf [6] = 1;

	if (mpu9250_conf [6] & 1)
		options |= MPU9250_SEN_ACCEL;
	if (mpu9250_conf [6] & 2)
		options |= MPU9250_SEN_GYRO;
	if (mpu9250_conf [6] & 4)
		options |= MPU9250_SEN_COMPASS;
	if (mpu9250_conf [6] & 8)
		options |= MPU9250_SEN_TEMP;

	// Include sampling rate
	options |= ((lword) mpu9250_conf [5]) << 24;

	// Forced low power mode
	if ((mpu9250_conf [0] & 0x01))
		// Low-power bit
		options |= MPU9250_LP_MODE;

	mpu9250_on (options, mpu9250_conf [1]);

	if (mpu9250_desc.evtype == 1) {
		if ((mpu9250_conf [0] & 0x08))
			// Event reports on motion
			mpu9250_desc . evtype |= 0x80;
		// Start the sampler to collect (and optionally report) motion
		// events
		if (!running (mpu9250_sampler))
			runfsm mpu9250_sampler;
		// This is used to tell the size of the data produced by the
		// sensor
		mpu9250_desc.components = 16;
	} else {
		mpu9250_desc.components = mpu9250_conf [6];
	}

	_BIS (Sensors, MPU9250_FLAG);
}

static void sensor_off_mpu9250 () {

	if (!mpu9250_active)
		return;

	mpu9250_off ();

	killall (mpu9250_sampler);

	_BIC (Sensors, MPU9250_FLAG);
}

// ============================================================================

fsm hdc1000_sampler {

	state HD_LOOP:

		read_hdc1000 (HD_LOOP, hdc1000_desc.values);
		delay (hdc1000_desc.smplint, HD_LOOP);
}

static void sensor_on_hdc1000 () {

	word options;

	if (hdc1000_active)
		return;

	options = scaled_option (hdc1000_accuracy, hdc1000_conf [1]);

	if (hdc1000_conf [0])
		options |= HDC1000_MODE_HEATER;

	if (hdc1000_conf [3] == 0)
		hdc1000_conf [3] = 1;
	if (hdc1000_conf [3] & 1)
		options |= HDC1000_MODE_HUMID;
	if (hdc1000_conf [3] & 2)
		options |= HDC1000_MODE_TEMP;

	bzero (&hdc1000_desc, sizeof (hdc1000_desc));

	// Sanity check included
	hdc1000_desc.smplint = hdc1000_conf [2] ? hdc1000_conf [2] : 1024;
	hdc1000_desc.components = hdc1000_conf [3];

	hdc1000_on (options);

	if (!running (hdc1000_sampler))
		runfsm hdc1000_sampler;

	_BIS (Sensors, HDC1000_FLAG);
}

static void sensor_off_hdc1000 () {

	if (!hdc1000_active)
		return;

	hdc1000_off ();

	killall (hdc1000_sampler);

	_BIC (Sensors, HDC1000_FLAG);
}

// ============================================================================

static void sensor_on_obmicrophone () {

	if (obmicrophone_active)
		return;

	// Sanity check
	if (obmicrophone_conf [0] < 100)
		obmicrophone_conf [0] = 100;
	// In kilohertz
	obmicrophone_on (obmicrophone_conf [0] * 10);

	_BIS (Sensors, OBMICROPHONE_FLAG);
}

static void sensor_off_obmicrophone () {

	if (!obmicrophone_active)
		return;

	obmicrophone_off ();

	_BIC (Sensors, OBMICROPHONE_FLAG);
}

// ============================================================================

fsm opt3001_sampler {

	state OP_LOOP:

		read_opt3001 (OP_LOOP, opt3001_desc.values);
		delay (opt3001_desc.smplint, OP_LOOP);
}

static void sensor_on_opt3001 () {

	if (opt3001_active)
		return;

	opt3001_on (scaled_option (opt3001_accuracy, opt3001_conf [1]) |
		OPT3001_MODE_AUTORANGE | (opt3001_conf [0] ?
			OPT3001_MODE_CMODE_CN : OPT3001_MODE_CMODE_SS));

	// For now, we only use autorange

	bzero (&opt3001_desc, sizeof (opt3001_desc));

	// Sanity check included
	opt3001_desc.smplint = opt3001_conf [2] ? opt3001_conf [2] : 1024;

	if (!running (opt3001_sampler))
		runfsm opt3001_sampler;

	_BIS (Sensors, OPT3001_FLAG);
}

static void sensor_off_opt3001 () {

	if (!opt3001_active)
		return;

	opt3001_off ();

	killall (opt3001_sampler);

	_BIC (Sensors, OPT3001_FLAG);
}

// ============================================================================

fsm bmp280_sampler {

	state BM_LOOP:

		read_bmp280 (BM_LOOP, (address)(bmp280_desc.values));
		delay (bmp280_desc.smplint, BM_LOOP);
}

static void sensor_on_bmp280 () {

	word options;

	if (bmp280_active)
		return;

	options = scaled_option (bmp280_rates, bmp280_conf [2]) |
		  scaled_option (bmp280_accuracy, bmp280_conf [1]) |
		  scaled_option (bmp280_bandwidth, bmp280_conf [3]) |
		  (bmp280_conf [0] ? BMP280_MODE_FORCED : BMP280_MODE_NORMAL);

	if (bmp280_conf [5] == 0)
		bmp280_conf [5] = 1;

	if ((bmp280_conf [5] & 1) == 0)
		// Mask out the pressure
		options &= ~BMP280_MODE_PRESS_OVS_MASK;

	if ((bmp280_conf [5] & 2) == 0)
		// Mask out the temp
		options &= ~BMP280_MODE_TEMP_OVS_MASK;

	bzero (&bmp280_desc, sizeof (bmp280_desc));

	bmp280_desc.components = bmp280_conf [5];

	bmp280_desc.smplint = bmp280_conf [4] ?  bmp280_conf [4] : 1024;

	bmp280_on (options);

	if (!running (bmp280_sampler))
		runfsm bmp280_sampler;

	_BIS (Sensors, BMP280_FLAG);
}

static void sensor_off_bmp280 () {

	if (!bmp280_active)
		return;

	bmp280_off ();

	killall (bmp280_sampler);

	_BIC (Sensors, BMP280_FLAG);
}

// ============================================================================

static word configure_sensor (word *opt, const byte *opl, sint nopt,
						const byte *pmt, sint *pml) {
	sint par;
	word val;
	byte pmask;

	if (*pml == 0)
		return ACK_PARAM;

	pmask = *pmt++;
	(*pml)--;

	for (par = 0; par < 8; par++, pmask >>= 1) {
		if ((pmask & 1)) {
			// The parameter is present
			if (par >= nopt || *pml == 0)
				return ACK_PARAM;
			val = *pmt++;
			(*pml)--;
			if (opl [par]) {
				// Second byte, big endian
				if (*pml == 0)
					return ACK_PARAM;
				val = (val << 8) | *pmt++;
				(*pml)--;
			}
			opt [par] = val;
		}
	}

	return ACK_OK;
}

word sensing_configure (const blob *cdt, sint lft) {
//
// Configure sensors
//
	sint len;
	word sen;
	const byte *buf;

	if (lft < cdt->size + 2)
		return ACK_LENGTH;

	buf = cdt->content;
	lft = cdt->size;

	while (lft) {
		// Sensor number
		sen = *buf++;
		len = --lft;
		switch (sen) {
			case MPU9250_INDEX:
				sen = configure_sensor (
					mpu9250_conf,
					mpu9250_clen,
					sizeof (mpu9250_conf),
					buf, &len);
				break;
			case HDC1000_INDEX:
				sen = configure_sensor (
					hdc1000_conf,
					hdc1000_clen,
					sizeof (hdc1000_conf),
					buf, &len);
				break;
			case OBMICROPHONE_INDEX:
				sen = configure_sensor (
					obmicrophone_conf,
					obmicrophone_clen,
					sizeof (obmicrophone_conf),
					buf, &len);
				break;
			case OPT3001_INDEX:
				sen = configure_sensor (
					opt3001_conf,
					opt3001_clen,
					sizeof (opt3001_conf),
					buf, &len);
				break;
			case BMP280_INDEX:
				sen = configure_sensor (
					bmp280_conf,
					bmp280_clen,
					sizeof (bmp280_conf),
					buf, &len);
				break;
			default:
				return ACK_PARAM;
		}

		if (sen)
			return sen;

		buf += (lft - len);
		lft = len;
	}

	return ACK_OK;
}

// ============================================================================

typedef	void (*sen_turn_fun_t)();

static trueconst sen_turn_fun_t sen_turn_fun [2] [NUMBER_OF_SENSORS] = { 
	 {
		sensor_on_mpu9250, 
		sensor_on_obmicrophone,
		sensor_on_bmp280,
		sensor_on_hdc1000,
		sensor_on_opt3001,
	},
	{
		sensor_off_mpu9250,
		sensor_off_obmicrophone,
		sensor_off_bmp280,
		sensor_off_hdc1000,
		sensor_off_opt3001,
	}
};

word sensing_turn (byte s) {
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

	if (Sensors == 0 && Status == STATUS_SAMPLING)
		sampling_stop ();

	if (!mpu9250_active && Status == STATUS_STREAMING)
		streaming_stop ();

	// Check if not void ...
	return ACK_OK;
}

word sensing_getconf (byte *where) {
//
// Return the configuration of all sensors in where
//
	sint i, j;
	word sl, nb, mb;
	word *st;
	const byte *ss;
	

	// Initialize the length
	nb = 0;

#define	setting(a)	st = a##_conf; ss = a##_clen; sl = sizeof (a##_clen); \
			break;

	for (i = 0; i < NUMBER_OF_SENSORS; i++) {
		switch (i) {
			case MPU9250_INDEX: 	   setting (mpu9250);
			case OBMICROPHONE_INDEX:   setting (obmicrophone);
			case BMP280_INDEX: 	   setting (bmp280);
			case HDC1000_INDEX: 	   setting (hdc1000);
			case OPT3001_INDEX: 	   setting (opt3001);
		}
#undef	setting
		if (where) {
			where [nb] = (byte) i;
			where [mb = nb + 1] = 0;
		}
		nb += 2;
		for (j = 0; j < sl; j++) {
			if (ss [j]) {
				// Two bytes
				if (where)
					where [nb] = (byte)(st [j] >> 8);
				nb++;
			}
			if (where) {
				where [nb] = (byte)st [j];
				where [mb] |= (1 << j);
			}
			nb++;
		}
	}

	return nb;
}

word sensing_report (byte *where, address mask) {
//
// Returns the sensor report
//
	word nb, cb;

	nb = 0;

	if (where)
		// Not a dry run
		*mask = 0;

	if (Sensors & MPU9250_FLAG) {
		// The IMU is on
		if (mpu9250_desc.components & 0x10) {
			// Motion report only
			nb += 2;
			if (where) {
				memcpy (where, &(mpu9250_desc.motion_events),
					2);
				// Zero out motion count
				mpu9250_desc.motion_events = 0;
				where += 2;
				*mask |= 0x10;
			}
		} else {
			nb += (cb =
			    mpu9250_desc_length [mpu9250_desc.components]);
			if (where) {
				read_mpu9250 (WNONE, (address) where);
				where += cb;
				*mask |= mpu9250_desc.components;
			}
		}
	}

	if (Sensors & OBMICROPHONE_FLAG) {
		nb += 8;
		if (where) {
			read_obmicrophone (WNONE, (address) where);
			obmicrophone_reset ();
			where += 8;
			*mask |= 1 << 7;
		}
	}

	if (Sensors & BMP280_FLAG) {
		nb += (cb = (bmp280_desc.components == 3) ? 8 : 4);
		if (where) {
			memcpy (where, bmp280_desc.values, cb);
			where += cb;
			*mask |= (bmp280_desc.components << 9);
		}
	}

	if (Sensors & HDC1000_FLAG) {
		nb += (cb = (hdc1000_desc.components == 3) ? 4 : 2);
		if (where) {
			memcpy (where, hdc1000_desc.values, cb);
			where += cb;
			*mask |= (hdc1000_desc.components << 5);
		}
	}

	if (Sensors & OPT3001_FLAG) {
		nb += 2;
		// We only return the first word
		if (where) {
			*((address)where) = opt3001_desc.values [0];
			where += 2;
			*mask |= 1 << 8;
		}
	}

	return nb;
}

#if EMULATE_SENSORS

// ============================================================================
// Sensor emulation
// ============================================================================

#ifdef	__SMURPH__
// Use the actual virtual sensor modeled by VUEE
#define	rds(a,b,c) read_sensor (a, b, c)
#else
// Fake the sensor
static byte les_value = 0;
static void rds (word st, word sn, address ret)	{ *ret = (word)(les_value++); }
#endif

void ready_mpu9250 (word st) {

	delay (mpu9250_conf [5] + 1, st);
	release;
}

void read_mpu9250 (word st, address ret) {

	word tmp;
	sint i, nc;

	rds (st, SENSOR_MPU9250, &tmp);

	if (!mpu9250_active) {
		ret [0] = 0;
		return;
	}

	// The value: 0-255 is transformed into a 16-bit FP int
	tmp |= (tmp << 8);

	if (mpu9250_desc.components == 0x10)
		// Motion detect mode
		nc = 3;		// 3 words
	else
		nc = mpu9250_data_size / 2;

	for (i = 0; i < nc; i++)
		ret [i] = tmp;
}

void read_hdc1000 (word st, address ret) {

	word tmp;
	sint i, nc;

	rds (st, SENSOR_HDC1000, &tmp);

	if ((nc = hdc1000_data_size / 2) == 0) {
		ret [0] = 0;
		return;
	}

	if ((tmp >>= 1) > 99)
		// Make this look like temperature of % humidity
		tmp = 99;
	tmp *= 100;

	for (i = 0; i < nc; i++)
		ret [i] = tmp;
}

void read_obmicrophone (word st, address ret) {

	word tmp;

	rds (st, SENSOR_OBMICROPHONE, &tmp);

	tmp |= (tmp << 8);

	if (obmicrophone_active) {
		((lword*) ret) [0] = (lword) tmp;
		((lword*) ret) [1] = (lword) tmp;
	} else {
		((lword*) ret) [0] = ((lword*) ret) [1] = 0;
	}
}

void read_opt3001 (word st, address ret) {

	word tmp;

	rds (st, SENSOR_OPT3001, &tmp);

	if (opt3001_active) {
		ret [0] = tmp;
		ret [1] = 0;
	} else {
		ret [0] = 0;
	}
}

void read_bmp280 (word st, address ret) {

	word tmp;
	sint i, nc;

	rds (st, SENSOR_BMP280, &tmp);

	if ((nc = bmp280_data_size/2) == 0) {
		((lword*)ret) [0] = 0;
		return;
	}
	
	tmp |= (tmp << 8);

	for (i = 0; i < nc; i++)
		((lword*) ret) [i] = tmp;
}
	
#endif

