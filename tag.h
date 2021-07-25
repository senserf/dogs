#ifndef __pg_tag_h
#define	__pg_tag_h

#include "sysio.h"

#define	ACTIVATING_BUTTON	0		// Hibernate, radio control
#define	HIBERNATE_ON_PUSH	5		// Seconds to hold the button

#define	STATUS_IDLE		0
#define	STATUS_SAMPLING		1
#define	STATUS_STREAMING	2

extern byte	Status;

fsm delayed_switch (byte);

#endif
