/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/
#ifndef	__pg_sensing_h
#define	__pg_sensing_h

//+++ sensing.cc

#include "sysio.h"
#include "ossi.h"

#if defined(__SMURPH__) || !defined(MPU9250_I2C_ADDRESS)
// The SENSORTAG-specific sensors have to be emulated if we are not running on
// the actual SENSORTAG device
#define	EMULATE_SENSORS		1
#else
#define	EMULATE_SENSORS		0
#endif

#if EMULATE_SENSORS && !defined(__SMURPH__)
// This means that we need the "emulation" headers from the sensor drivers, so
// we pretend we are in VUEE while they are being included
#define	__SMURPH__
#define	__VUEE_fake__
#endif

#include "mpu9250.h"
#include "obmicrophone.h"
#include "bmp280.h"
#include "hdc1000.h"
#include "opt3001.h"

#ifdef	__VUEE_fake__
#undef	__SMURPH__
#undef	__VUEE_fake__
#endif

#define	NUMBER_OF_SENSORS	5

// System identifiers
#define	SENSOR_BATTERY		(-1)
#define	SENSOR_MPU9250		1
#define	SENSOR_OBMICROPHONE	2
#define	SENSOR_BMP280		3
#define	SENSOR_HDC1000		4
#define	SENSOR_OPT3001		5

#define	SENSOR_BATTERY		(-1)

// Logical indexes for the application
#define	MPU9250_INDEX		0
#define OBMICROPHONE_INDEX	1
#define BMP280_INDEX		2
#define HDC1000_INDEX		3
#define OPT3001_INDEX		4

#define	MPU9250_FLAG		(1 << MPU9250_INDEX)
#define HDC1000_FLAG		(1 << HDC1000_INDEX)
#define OBMICROPHONE_FLAG	(1 << OBMICROPHONE_INDEX)
#define OPT3001_FLAG		(1 << OPT3001_INDEX)
#define BMP280_FLAG		(1 << BMP280_INDEX)

// ============================================================================
// MPU9250
// ============================================================================

#define	MPU9250_PAR_MOTION	0	// Offsets
#define	MPU9250_PAR_THRESHOLD	1
#define	MPU9250_PAR_RATE	2
#define	MPU9250_PAR_ACCURACY	3
#define	MPU9250_PAR_BANDWIDTH	4
#define	MPU9250_PAR_COMPONENTS	5
#define	MPU9250_PAR_REPORT	6

typedef struct {
	byte motion, components;
	// Only needed in motion detection mode
	word motion_events;
} mpu9250_desc_t;

#define	mpu9250_active		(Sensors & MPU9250_FLAG)
#define	mpu9250_data_size	(mpu9250_active ? \
			 	 mpu9250_desc_length [mpu9250_desc.components] \
				 : 0)

// ============================================================================
// HDC1000
// ============================================================================

#define	HDC1000_PAR_HEATER	0
#define	HDC1000_PAR_ACCURACY	1
#define	HDC1000_PAR_COMPONENTS	2

typedef struct {
	byte components;
	word smplint;	// Sampling interval
	word values [2];
} hdc1000_desc_t;

#define	hdc1000_active		(Sensors & HDC1000_FLAG)
#define	hdc1000_data_size	(hdc1000_active ? (hdc1000_desc.components == \
				 3 ? 4 : 2) : 0)

// ============================================================================
// OBMICROPHONE
// ============================================================================

#define	obmicrophone_active		(Sensors & OBMICROPHONE_FLAG)
#define	obmicrophone_data_size		(obmicrophone_active ? 8 : 0)

// ============================================================================
// OPT3001
// ============================================================================

typedef struct {

	word smplint;	// Sampling interval
	word values [2];

} opt3001_desc_t;

#define	opt3001_active			(Sensors & OPT3001_FLAG)
#define	opt3001_data_size		(opt3001_active ? 2 : 0)

// ============================================================================
// BMP280
// ============================================================================

typedef struct {

	byte components;
	word smplint;	// Sampling interval
	lword values [2];

} bmp280_desc_t;

#define	bmp280_active		(Sensors & BMP280_FLAG)
#define	bmp280_data_size	(bmp280_active ? (bmp280_desc.components == \
				 3 ? 8 : 4) : 0)

extern byte Sensors;
extern mpu9250_desc_t mpu9250_desc;

word sensing_turn (byte);
word sensing_configure (const command_config_t*, sint);
word sensing_status (byte*);
word sensing_report (byte*, address);

#define	sensing_all_off()	sensing_turn (0x00)

#if EMULATE_SENSORS

// ============================================================================
// Sensor emulation; will have to provide these functions
// ============================================================================

void read_mpu9250 (word, address);
void read_hdc1000 (word, address);
void read_opt3001 (word, address);
void read_bmp280 (word, address);
void read_obmicrophone (word, address);

#else

// ============================================================================
// THE REAL WORLD
// ============================================================================

#define read_mpu9250(s,p)	read_sensor (s, SENSOR_MPU9250, p)
#define read_hdc1000(s,p)	read_sensor (s, SENSOR_HDC1000, p)
#define read_opt3001(s,p)	read_sensor (s, SENSOR_OPT3001, p)
#define read_bmp280(s,p)	read_sensor (s, SENSOR_BMP280, p)
#define read_obmicrophone(s,p)	read_sensor (s, SENSOR_OBMICROPHONE, p)

#endif

#endif
