#if	defined(SIGNALLING_LEDS) && !defined(__leddefs_h__)

#define	__leddefss_h__

word	__sig_bdelay [SIGNALLING_LEDS], __sig_nblinks [SIGNALLING_LEDS];

fsm __sig_blinker (word led) {

	state BL_UPD:
		// We need the third state because we want nblinks to act as
		// a semaphore
		if (__sig_nblinks [led])
			__sig_nblinks [led]--;

	initial state BL_START:

		if (__sig_nblinks [led]) {
			leds (led, 1);
			delay (__sig_bdelay [led], BL_NEXT);
			release;
		}

		finish;

	state BL_NEXT:

		leds (led, 0);
		delay (__sig_bdelay [led], BL_UPD);
}

static void blink (word led, word n, word d) {

	word sb;

	if (led >= SIGNALLING_LEDS)
		// Play it safe
		return;

	// This tells us whether the process is running
	sb = __sig_nblinks [led];

	if (n == 0) {
		// Stop
		if (sb > 1)
			// Don't set it below 1 because the nonzero value means
			// that the process is up and running
			__sig_nblinks [led] = 1;
		return;
	}

	if ((n += sb) > 255)
		// Not too many at once
		n = 255;

	if (d > 1024)
		// One second is max
		d = 1024;
	else if (d < 4)
		d = 4;

	__sig_bdelay [led] = d;
	__sig_nblinks [led] = n;

	if (sb == 0)
		// Start the process
		runfsm __sig_blinker (led);
}

#endif
