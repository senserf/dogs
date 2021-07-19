/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "sensing.h"
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
	// Based on component selection: AGCT; actually TCGA treated as bits
	// in a value 0-15, so A = 1, CG = 6, and so on:
	// .... = 0, ...A = 6, ..G. = 6, ..GA = 12, ...
	0, 6, 6, 12, 6, 12, 12, 18, 2, 8, 8, 14, 8, 14, 14, 20, 4
};

static byte mpu9250_conf [] = { NO, 4, 4, 6, 3, 1, NO };

mpu9250_desc_t mpu9250_desc;

// ============================================================================
// HDC1000
// ============================================================================

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

static byte hdc1000_conf [] = { NO, 7, 1, 4 };	// Sampled in BGR

static hdc1000_desc_t hdc1000_desc;

// ============================================================================
// OBMICROPHONE
// ============================================================================

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

static byte obmicrophone_conf [1] = { 4 };

// ============================================================================
// OPT3001
// ============================================================================

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

static byte opt3001_conf [] = { NO, 3, 4 };		// Sampled in BGR

static opt3001_desc_t opt3001_desc;

// ============================================================================
// BMP280
// ============================================================================

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

static byte bmp280_conf [] = { YES, 7, 4, 7, 1, 4 };	// Sampled in BGR

static bmp280_desc_t bmp280_desc;

// Active sensors
byte Sensors;

// ============================================================================

static word scaled_option (const word *t, byte v) {
//
// To play it safe
//
	if (v > 7)
		v = 7;
	return t [v];
}

// ============================================================================

fsm mpu9250_sampler {
//
// This is used for detecting motion events; otherwise, the sensor is sampled
// at the moment of report. The FSM can only be running if mpu9250_desc.motion
// is nonzero.
//
	state MP_MOTION:

		word values [3];
		read_sensor (MP_MOTION, SENSOR_MPU9250, values);

		// The number of motion events
		mpu9250_desc.motion_events ++;

		ossint_motion_event (values, mpu9250_desc.motion_events);

	initial state MP_LOOP:

		wait_sensor (SENSOR_MPU9250, MP_MOTION);
		release;
}

static void sensor_on_mpu9250 () {

	word options;

	if (mpu9250_active)
		// The sensor is on, do nothing
		return;

	options = scaled_option (mpu9250_rates, mpu9250_conf [2]) |
		  scaled_option (mpu9250_accuracy, mpu9250_conf [3]) |
		  scaled_option (mpu9250_bandwidth, mpu9250_conf [4]);

	if (mpu9250_conf [0]) {
		options |= MPU9250_LP_MOTION_DETECT | MPU9250_SEN_ACCEL;
		// This is a special configuration of components
		mpu9250_conf [5] = 0x10;
	} else {
		if (mpu9250_conf [5] == 0 || mpu9250_conf [5] > 0xf) { 
			// Make sure there is at least one component, i.e.,
			// accel, for sanity
			mpu9250_conf [5] = 1;
		}
		if ((mpu9250_conf [5] & 1) || mpu9250_conf [0])
			options |= MPU9250_SEN_ACCEL;
		if (mpu9250_conf [5] & 2)
			options |= MPU9250_SEN_GYRO;
		if (mpu9250_conf [5] & 4)
			options |= MPU9250_SEN_COMPASS;
		if (mpu9250_conf [5] & 8)
			options |= MPU9250_SEN_TEMP;
	}

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

	if (!mpu9250_active)
		return;

	mpu9250_off ();

	killall (mpu9250_sampler);

	_BIC (Sensors, MPU9250_FLAG);
}

// ============================================================================

fsm hdc1000_sampler {

	state HD_LOOP:

		read_sensor (HD_LOOP, SENSOR_HDC1000, hdc1000_desc.values);
		delay (hdc1000_desc.smplint, HD_LOOP);
}

static void sensor_on_hdc1000 () {

	word options;

	if (hdc1000_active)
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

	if (!hfc1000_active)
		return;

	hdc1000_off ();

	killall (hdc1000_sampler);

	_BIC (Sensors, HDC1000_FLAG);
}

// ============================================================================

static void sensor_on_obmicrophone () {

	if (obmicrophone_active)
		return;

	obmicrophone_on (scaled_option (obmicrophone_rates,
		obmicrophone_conf [0]));

	// This one is not sampled in the background

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

		read_sensor (OP_LOOP, SENSOR_OPT3001, opt3001_desc.values);
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
	opt3001_desc.smplint = scaled_option (smpl_intervals,
		opt3001_conf [2]);

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

		read_sensor (BM_LOOP, SENSOR_BMP280,
			(address)(bmp280_desc.values));
		delay (bmp280_desc.smplint, BM_LOOP);
}

static void sensor_on_bmp280 () {

	word options;

	if (bmp280_active)
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

	if (!bmp280_active == 0)
		return;

	bmp280_off ();

	killall (bmp280_sampler);

	_BIC (Sensors, BMP280_FLAG);
}

// ============================================================================

static word configure_sensor (byte *opt, sint nopt, byte *pmt, word pml) {

	sint par, val;

	while (pml--) {
		par = (*pmt >> 4) & 0x0f;
		val = (*pmt & 0x0f);
		if (par >= nopt)
			return ACK_FMT;
		opt [par] = (byte) val;
		pmt++;
	}

	return ACK_OK;
}

word sensing_configure (const byte *buf, sint lft) {
//
// Configure sensors
//
	sint len;
	word sen;

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
				sen = configure_sensor (mpu9250_conf,
					sizeof (mpu9250_conf), buf, len);
				break;
			case HDC1000_INDEX:
				sen = configure_sensor (hdc1000_conf,
					sizeof (hdc1000_conf), buf, len);
				break;
			case OBMICROPHONE_INDEX:
				sen = configure_sensor (obmicrophone_conf,
					sizeof (obmicrophone_conf), buf, len);
				break;
			case OPT3001_INDEX:
				sen = configure_sensor (opt3001_conf,
					sizeof (opt3001_conf), buf, len);
				break;
			case BMP280_INDEX:
				sen = configure_sensor (bmp280_conf,
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
	return ACK_OK;
}

// ============================================================================

static void (*sen_turn_fun [2][NUMBER_OF_SENSORS])() = { 
	 { sensor_on_mpu9250, sensor_on_hdc1000, sensor_on_obmicrophone,
		sensor_on_opt3001, sensor_on_bmp280 },
	 { sensor_off_mpu9250, sensor_off_hdc1000, sensor_off_obmicrophone,
		sensor_off_opt3001, sensor_off_bmp280 }
};

void sensing_turn (byte s) {
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
}

word sensing_status (byte *where) {
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

	if (Sensors & HDC1000_FLAG) {
		nb += (cb = (hdc1000_desc.components == 3) ? 4 : 2);
		if (where) {
			memcpy (where, hdc1000_desc.values, cb);
			where += cb;
			*mask |= (hdc1000_desc.components << 5);
		}
	}

	if (Sensors & OBMICROPHONE_FLAG) {
		nb += 8;
		if (where) {
			read_sensor (WNONE, SENSOR_OBMICROPHONE,
			    (address) where);
			obmicrophone_reset ();
			where += 8;
			*mask |= 1 << 7;
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

	if (Sensors & BMP280_FLAG) {
		nb += (cb = (bmp280_desc.components == 3) ? 8 : 4);
		if (where) {
			memcpy (where, bmp280_desc.values, cb);
			where += cb;
			*mask |= (bmp280_desc.components << 9);
		}
	}

	return nb;
}
