#include "ledblink.h"

typedef	struct {
	word bdelay, nblinks;
} ledblink_t;

static ledblink_t *all_leds;
static sint	   nleds;

fsm sig_blinker (word led) {

	state BL_UPD:
		// We need the third state because we want nblinks to act as
		// a semaphore
		if (all_leds [led] . nblinks)
			all_leds [led] . nblinks --;

	initial state BL_START:

		if (all_leds [led] . nblinks) {
			leds (led, 1);
			delay (all_leds [led] . bdelay, BL_NEXT);
			release;
		}

		finish;

	state BL_NEXT:

		leds (led, 0);
		delay (all_leds [led] . bdelay, BL_UPD);
}

void led_blink (word led, word n, word d) {

	word sb;

	if (led >= nleds)
		// Play it safe
		return;

	// This tells us whether the process is running
	sb = all_leds [led] . nblinks;

	if (n == 0) {
		// Stop
		if (sb > 1)
			// Don't set it below 1 because the nonzero value means
			// that the process is up and running
			all_leds [led] . nblinks = 1;
		return;
	}

#ifdef __SMURPH__
	emul (1, "LED BLINK: %1u = (%1u, %1u)", led, n, d);
	if (n > 2)
		n = 2;
	if (d < 512)
		d = 512;
#endif
	if ((n += sb) > 255)
		// Not too many at once
		n = 255;

	if (d > 1024)
		// One second is max
		d = 1024;
	else if (d < 4)
		d = 4;

	all_leds [led] . bdelay = d;
	all_leds [led] . nblinks = n;

	if (sb == 0)
		// Start the process
		runfsm sig_blinker (led);
}

void led_stop () {

	sint i;

if (crunning (sig_blinker))
	killall (sig_blinker);
	bzero (all_leds, nleds * sizeof (ledblink_t));
	for (i = 0; i < nleds; i++)
		leds (i, 0);
}

void led_init (sint nl) {

	if ((all_leds = (ledblink_t*) umalloc ((nleds = nl) *
		sizeof (ledblink_t))) == NULL)
			syserror (ERESOURCE, "led");

	led_stop ();
}	
