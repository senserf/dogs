/*
	Copyright 2002-2021 (C) Olsonet Communications Corporation
	Programmed by Pawel Gburzynski
	All rights reserved

	This file is part of the PICOS platform

*/
#include "tag.h"
#include "rf.h"
#include "ossint.h"
#include "sensing.h"
#include "streaming.h"
#include "sampling.h"
#include "ledsignal.h"

// ============================================================================
// ============================================================================

byte	Status = STATUS_IDLE,	// Doing what
	RadioActiveCD;

word	Voltage;

static	byte  MonStat, MonWake, MonRef, BatCnt = 1;

#define	MS_ON		0
#define	MS_OFF		1

// ============================================================================

fsm rf_monitor {

	state RFM_ON:

		delay (ACT_MONITOR_INTERVAL, RFM_RUN_ON);
		release;

	state RFM_RUN_ON:

#ifndef __SMURPH__
		if (RadioActiveCD >= ACT_COUNTDOWN) {
			// Turn off
			MonStat = MS_OFF;
			sampling_stop ();
			streaming_stop ();
			sensing_all_off ();
			tcv_control (RFC, PHYSOPT_OFF, NULL);
			sameas RFM_OFF;
		}

		RadioActiveCD++;
#endif
		if (--BatCnt != 0)
			sameas RFM_ON;

	state RFM_BATTMON:

		read_sensor (RFM_BATTMON, SENSOR_BATTERY, &Voltage);
		BatCnt = ACT_BATTMON_FREQ;
		sameas RFM_ON;

	state RFM_OFF:

		MonWake = 0;
		delay (ACT_RXOFF_INTERVAL, RFM_RUN_OFF);
		release;

	state RFM_RUN_OFF:

		tcv_control (RFC, PHYSOPT_ON, NULL);
		delay (ACT_RXON_INTERVAL, RFM_CHECK_WAKE);
		release;

	state RFM_CHECK_WAKE:

		if (RadioActiveCD == 0) {
			// Activity
			if (MonWake) {
				sameas RFM_WACK;
			} else {
				MonStat = MS_ON;
				sameas RFM_ON;
			}
		}

		// No activity
		tcv_control (RFC, PHYSOPT_OFF, NULL);
		sameas RFM_OFF;

	state RFM_WACK:

		// Wait until the wake messages are gone
		if (MonWake) {
			MonWake = 0;
			delay (ACT_WCLEAR_INTERVAL, RFM_WACK);
			release;
		}

		// Acknowledge the WAKE
		osscmn_xack (MonRef, ACK_OK);
		MonStat = MS_ON;
		sameas RFM_ON;
}

static void handle_wake (byte ref) {

	if (MonStat == MS_ON) {
		osscmn_xack (ref, ACK_VOID);
	} else {
		MonWake = 1;
		MonRef = ref;
	}
}

// ============================================================================

fsm delayed_switch {

	state DS_START:

		// We hibernate: blink the leds and delay for a sec
		led_signal (0, 16, 72);
		delay (1024, DS_SWITCH);
		release;

	state DS_SWITCH:

		tcv_control (RFC, PHYSOPT_OFF, NULL);
		sensing_all_off ();
		delay (1024, DS_HIBERNATE);
		release;

	state DS_HIBERNATE:

		led_stop ();
		hibernate ();
}

// ============================================================================

void do_mreg (const command_mreg_t *par, word pml) {

	byte a, r;
	word bsize;
	address msg;
	message_mreg_t *pmt;

	if (pml < 3)
		return;

	if (par->what) {
		// Write
		mpu9250_wrega (par->regn, par->value);
		return;
	}

	if (par->value < 1 || par->value > 32)
		// A sanity check
		return;

	if ((msg = osscmn_xpkt (message_mreg_code, LastRef,
	   sizeof (message_mreg_t) + par->value)) == NULL)
		return;

	pmt = (message_mreg_t*) pkt_payload (msg);
	pmt->data.size = par->value;
	mpu9250_rregan (par->regn, pmt->data.content, par->value);
	tcv_endpx (msg, YES);
}

// ============================================================================

void handle_rf_packet (byte code, byte ref, const address par, word pml) {

	word ret;
	address msg;

	if (code == MESSAGE_CODE_STRACK) {
		// Ignore ref
		streaming_tack (ref, (byte*) par, pml);
		return;

	}

	if (code == command_wake_code) {
		// Ignore ref
		handle_wake (ref);
		return;
	}

	// Ignore if duplicate
	if (ref == LastRef)
		return;

	LastRef = ref;

	// 4 msecs of breathing space for the peg
	ret = 2;
	tcv_control (RFC, PHYSOPT_CAV, &ret);

	switch (code) {

		case command_config_code:

			if (((const command_config_t*) par)->confdata . size ==
			 	0) {
				if ((ret = ossint_send_config ()) == ACK_OK)
					return;
			} else {
				// Configure sensors
				ret = sensing_configure (
			    		&(((const command_config_t*) par)->
						confdata), pml);
			}
			break;

		case command_onoff_code:

			ret = sensing_turn (*((byte*)par));
			break;

		case command_status_code:

			// Respond with status
			if ((ret = ossint_send_status ()) == ACK_OK)
				// No ACK if OK
				return;
			break;

		case command_sample_code:

			// Start sampling
			ret = sampling_start ((const command_sample_t*) par,
				pml);
			break;

		case command_stream_code:

			// Start streaming
			ret = streaming_start ((const command_stream_t*) par,
				pml);
			if (ret == ACK_OK)
				// Don't send the ACK, the train is coming,
				// just return
				return;
			break;

		case command_stop_code:

			ret = ACK_OK;
			if (Status == STATUS_SAMPLING) {
				sampling_stop ();
			} else if (Status == STATUS_STREAMING) {
				streaming_stop ();
			} else
				ret = ACK_VOID;
			break;

		case command_mreg_code:

			do_mreg ((const command_mreg_t*) par, pml);
Ack:
			ret = ACK_OK;
			break;

		case command_wake_code:

			// Do nothing, wake up and acknowledge
			goto Ack;

		default:

			ret = ACK_COMMAND;
	}
			
	led_signal (0, ret + 1, 64);

	// Send the ack
	osscmn_xack (LastRef, ret);
}

// ============================================================================

fsm button_holder (sint counter) {

	state BH_LOOP:

		delay (10, BH_TRY);
		release;

	state BH_TRY:

		if (!button_down (ACTIVATING_BUTTON)) {
			// A short push, do nothing
			finish;
		}

		if ((counter -= 10) >= 0) {
			savedata (counter);
			sameas BH_LOOP;
		}

	state BH_HIBERNATE:

		if (!runfsm delayed_switch)
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

			runfsm button_holder (1024 * HIBERNATE_ON_PUSH);
			return;
	}

	// Ignore the other button for now
}
		
#ifndef __SMURPH__

fsm root (aword sflags) {

	state RS_INIT:

#if 1
		if (sflags == 0)
			// Start in hibernating state, wakeup on button
			hibernate ();
#endif
#else

fsm root {

	state RS_INIT:

#endif
		powerdown ();

		led_signal (0, 1, 128);
		// Initialize the interface in RF active state
		osscmn_init ();

		runfsm rf_monitor;

		buttons_action (buttons);

		// That's it, no more use for us
		finish;
}

// ============================================================================
// ============================================================================
