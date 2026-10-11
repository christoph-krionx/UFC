#ifndef leds_h
#define leds_h

#include <FastLED.h>
#include "Config.h"

class Leds
{
	public:
		void Init();
		void SetSwitch(uint8_t index, CRGB left, CRGB right);
		// writes the led chain only if something has changed
		void Update();

		static CRGB ColorOf(uint8_t color);

		// current content of the led chain, for diagnostics
		const CRGB * Data() { return _leds; }

	private:
		CRGB _leds[NUM_LEDS];
		bool _dirty;
};

#endif
