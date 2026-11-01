#ifndef leds_h
#define leds_h

#include <FastLED.h>
#include "Config.h"

class Leds
{
	public:
		void Init();
		void SetSwitch(uint8_t index, CRGB left, CRGB right);
		// writes the led chain if something has changed and repeats it twice afterwards,
		// the repetitions wait while repeatAllowed is false (writing blocks the interrupts)
		void Update(bool repeatAllowed);

		static CRGB ColorOf(uint8_t color);

		// current content of the led chain, for diagnostics
		const CRGB * Data() { return _leds; }

	private:
		CRGB _leds[NUM_LEDS];
		bool _dirty;
		uint32_t _shown;	// time of the last change
		uint8_t _repeats;	// repetitions done since the last change
};

#endif
