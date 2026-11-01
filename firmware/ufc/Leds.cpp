#include "Leds.h"

void Leds::Init()
{
	// keep the leds unpowered until the data line is defined
	pinMode(PIN_LED_VCC_ON, OUTPUT);
	digitalWrite(PIN_LED_VCC_ON, HIGH);

	FastLED.addLeds<WS2812B, PIN_LED_DATA, LED_COLOR_ORDER>(_leds, NUM_LEDS);
	FastLED.setBrightness(LED_BRIGHTNESS);
	FastLED.setMaxPowerInVoltsAndMilliamps(5, LED_MAX_MILLIAMPS);
	fill_solid(_leds, NUM_LEDS, CRGB::Black);

	delay(200);
	digitalWrite(PIN_LED_VCC_ON, LOW);
	delay(50);
	FastLED.show();
	_dirty = false;
	_repeats = 2;
}

void Leds::SetSwitch(uint8_t index, CRGB left, CRGB right)
{
	if (SWITCHES[index].ledLeft == NO_LED)
		return;

	// a single led shows the state color if it is lit, otherwise the function color
	if (SWITCHES[index].ledRight == NO_LED)
	{
		CRGB color = right ? right : left;

		if (_leds[SWITCHES[index].ledLeft] != color)
		{
			_leds[SWITCHES[index].ledLeft] = color;
			_dirty = true;
		}
		return;
	}

	if (_leds[SWITCHES[index].ledLeft] != left || _leds[SWITCHES[index].ledRight] != right)
	{
		_leds[SWITCHES[index].ledLeft] = left;
		_leds[SWITCHES[index].ledRight] = right;
		_dirty = true;
	}
}

void Leds::Update(bool repeatAllowed)
{
	if (_dirty)
	{
		FastLED.show();
		_dirty = false;
		_shown = millis();
		_repeats = 0;
		return;
	}

	if (_repeats >= 2 || !repeatAllowed)
		return;

	if (millis() - _shown >= (_repeats == 0 ? LED_REPEAT_1_MS : LED_REPEAT_2_MS))
	{
		FastLED.show();
		_repeats++;
	}
}

CRGB Leds::ColorOf(uint8_t color)
{
	switch (color)
	{
		case COLOR_RED:		return CRGB(255, 0, 0);
		case COLOR_ORANGE:	return CRGB(255, 80, 0);
		case COLOR_YELLOW:	return CRGB(255, 180, 0);
		case COLOR_GREEN:	return CRGB(0, 255, 0);
		case COLOR_CYAN:	return CRGB(0, 255, 255);
		case COLOR_BLUE:	return CRGB(0, 0, 255);
		case COLOR_PURPLE:	return CRGB(160, 0, 255);
		case COLOR_WHITE:	return CRGB(255, 255, 255);
		default:		return CRGB(0, 0, 0);
	}
}
