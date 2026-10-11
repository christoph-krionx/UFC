#include "Switches.h"

void Switches::Init()
{
	for (uint8_t i = 0; i < NUM_SWITCHES; i++)
	{
		pinMode(SWITCHES[i].pin, INPUT);
		_pressed[i] = false;
		_lastChange[i] = 0;
	}
}

bool Switches::Update(uint8_t index, bool & pressed)
{
	bool now = (digitalRead(SWITCHES[index].pin) == LOW);

	// ignore pin changes during the debounce time
	if (now == _pressed[index] || millis() - _lastChange[index] < SWITCH_DEBOUNCE_MS)
		return false;

	_pressed[index] = now;
	_lastChange[index] = millis();
	pressed = now;

	return true;
}
