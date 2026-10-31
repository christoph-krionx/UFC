#include "Switches.h"

void Switches::Init()
{
	for (uint8_t i = 0; i < NUM_SWITCHES; i++)
	{
		pinMode(SWITCHES[i].pin, INPUT);
		_pressed[i] = false;
		_raw[i] = false;
		_lastChange[i] = 0;
	}
}

bool Switches::Update(uint8_t index, bool & pressed)
{
	bool now = (digitalRead(SWITCHES[index].pin) == LOW);

	// every pin change restarts the debounce time, a new state counts once it is stable
	if (now != _raw[index])
	{
		_raw[index] = now;
		_lastChange[index] = millis();
		return false;
	}

	if (now == _pressed[index] || millis() - _lastChange[index] < SWITCH_DEBOUNCE_MS)
		return false;

	_pressed[index] = now;
	pressed = now;

	return true;
}
