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
#include "mpu9250.h"
#include "obmicrophone.h"
#include "bmp280.h"
#include "hdc1000.h"
#include "opt3001.h"

#define	NUMBER_OF_SENSORS	5

// System identifiers
#define	SENSOR_MPU9250		1
#define	SENSOR_OBMICROPHONE	2
#define	SENSOR_BMP280		3
#define	SENSOR_HDC1000		4
#define	SENSOR_OPT3001		5

#define	SENSOR_BATTERY		(-1)

// Logical indexes for the application
// ... and why exectly can't we use system identifiers? OK, who cares!
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

extern byte Sensors;

word sensing_turn (byte);
word sensing_configure (const byte*, sint);
word sensing_status (byte*);
word sensing_report (byte*, address);

#define	sensing_all_off ()	sensing_turn (0x00)

#ifndef
