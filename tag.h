#ifndef __pg_tag_h
#define	__pg_tag_h

#include "sysio.h"

#define	ACTIVATING_BUTTON	0		// Hibernate, radio control
#define	HIBERNATE_ON_PUSH	5		// Seconds to hold the button

#define	STATUS_IDLE		0
#define	STATUS_SAMPLING		1
#define	STATUS_STREAMING	2

extern byte	Status;
extern word	Voltage;

#define	VOLTAGE	((byte)(Voltage >> 3))

fsm delayed_switch (byte);

typedef struct {
//
// Parameters (accessible with the set command); we may be adding more
//
	word		train_length;
	word		max_queued;
	word		car_space;
	word		min_train_space;

} tag_params_t;

extern	tag_params_t	TagParams;

typedef	struct {
//
// Stream session statistics
//
	lword		fifo_overflows;
	lword		malloc_failures;
	lword		queue_drops;

} stream_stats_t;

extern	stream_stats_t	StreamStats;

#endif
