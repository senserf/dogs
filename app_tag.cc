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

byte	Status = STATUS_IDLE;	// Doing what

// ============================================================================

fsm delayed_switch (byte opn) {

	state DS_START:

		if (opn == RADIO_MODE_OFF || opn == RADIO_MODE_HIBERNATE) {
			// Radio goes off or hibernate; give it one second for
			// the ACK to get through and then proceed
			led_signal (0, opn ? 64 : 16, opn ? 72 : 150);
			delay (1024, DS_SWITCH);
			release;
		}

		// Fall through for normal radio switch

	state DS_RADIO:

		osscmn_turn_radio (opn);
		finish;

	state DS_SWITCH:

		if (opn != RADIO_MODE_HIBERNATE)
			sameas DS_RADIO;

		// Hibernate
		osscmn_turn_radio (RADIO_MODE_OFF);
		sensing_all_off ();

		// A bit more delay, so the LEDs finish blinking
		delay (64 * 72, DS_HIBERNATE);
		release;

	state DS_HIBERNATE:

		led_stop ();
		hibernate ();
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

	// Ignore if duplicate
	if (ref == LastRef)
		return;

	LastRef = ref;

	switch (code) {

		case command_config_code:

			// Configure sensors
			ret = sensing_configure ((const command_config_t*) par,
				pml);
			break;

		case command_onoff_code:

			ret = sensing_turn (*((byte*)par));
			break;

		case command_radio_code:

			ret = ossint_set_radio ((const command_radio_t*) par,
				pml);
			break;

		case command_status_code:

			// Respond with status
			if (running (ossint_sensor_status_sender)) {
				ret = ACK_BUSY;
				break;
			} else if (!runfsm ossint_sensor_status_sender) {
				ret = ACK_NORES;
				break;
			}
			return;

		case command_sample_code:

			// Start sampling
			ret = sampling_start ((const command_sample_t*) par,
				pml);
			break;

		case command_stream_code:

			// Start streaming
			ret = streaming_start ((const command_sample_t*) par,
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

		delay (1, BH_TRY);
		release;

	state BH_TRY:

		if (!button_down (ACTIVATING_BUTTON)) {
			// A short push. toggle radio
			ossint_toggle_radio ();
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

			runfsm button_holder (1024 * HIBERNATE_ON_PUSH);
			return;
	}

	// Ignore the other button for now
}
		
#ifndef __SMURPH__

fsm root (aword sflags) {

	state RS_INIT:

		if (sflags == 0)
			// Start in hibernating state, wakeup on button
			hibernate ();
#else

fsm root {

	state RS_INIT:

#endif

		led_init (1);

		powerdown ();
		// Initialize the interface in RF active state
		osscmn_init ();

		buttons_action (buttons);

		// That's it, no more use for us
		finish;
}

// ============================================================================
// ============================================================================
