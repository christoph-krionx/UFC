#include "Controller.h"
#include "Display.h"
#include "Storage.h"

// log lines are marked for the editor protocol
static void Log(const char * text)
{
	Serial.print(F("# "));
	Serial.println(text);
}

static void FormatMessage(const Message & message, bool on, char * text, uint8_t size)
{
	if (message.type == MSG_PC)
		snprintf(text, size, "PC %u C%u", message.number, message.channel + 1);
	else
		snprintf(text, size, "CC %u=%u C%u", message.number, on ? message.valueOn : message.valueOff, message.channel + 1);
}

void Controller::Init()
{
	Serial.begin(115200);
	Log("UFC " VERSION);

	// AREF is driven externally, never use the default reference on this board
	analogReference(EXTERNAL);

	_leds.Init();
	_switches.Init();
	_midi.Init();

	Display::Init();
	Display::PrintLine(0, "UFC " VERSION);

	// wait some time to show the version string on the lcd
	delay(750);

	memset(_on, 0, sizeof(_on));
	memset(_pressed, 0, sizeof(_pressed));
	_flashActive = false;
	_presetSavePending = false;
	_menuHold = false;
	_menu.Init(this);

	// holding the outer switches of the top row during power on restores the factory configuration
	bool resetRequested = (digitalRead(SWITCHES[18].pin) == LOW && digitalRead(SWITCHES[20].pin) == LOW);

	if (resetRequested || !Storage::IsValid())
	{
		Display::PrintLine(1, "Init memory...");
		Log("Init memory");
		Storage::Format();
	}

	Storage::LoadGlobal(_global);
	LoadPreset(Storage::LoadLastPreset());
	_presetSavePending = false;
}

void Controller::Update()
{
	bool pressed;

	for (uint8_t i = 0; i < NUM_SWITCHES; i++)
	{
		if (_switches.Update(i, pressed))
		{
			_pressed[i] = pressed;

			if (_menu.Active())
				_menu.OnSwitch(i, pressed);
			else
				OnSwitch(i, pressed);
		}
	}

	if (_menu.Active())
	{
		_menu.Update();
		_leds.Update();
		return;
	}

	// holding both menu switches opens the edit mode
	if (_pressed[MENU_SWITCH_A] && _pressed[MENU_SWITCH_B])
	{
		if (!_menuHold)
		{
			_menuHold = true;
			_menuHoldStart = millis();
		}
		else if (millis() - _menuHoldStart >= MENU_HOLD_MS)
		{
			_menuHold = false;
			_flashActive = false;
			_presetSavePending = false;
			_menu.Enter();
			_leds.Update();
			return;
		}
	}
	else
	{
		_menuHold = false;
	}

	if (_flashActive && millis() - _flashStart >= DISPLAY_FLASH_MS)
	{
		_flashActive = false;
		ShowStatusLine();
	}

	// the last used preset is stored with a delay to save eeprom write cycles while stepping through presets
	if (_presetSavePending && millis() - _presetChanged >= PRESET_SAVE_DELAY_MS)
	{
		_presetSavePending = false;
		Storage::SaveLastPreset(_presetIndex);
	}

	_leds.Update();
}

uint8_t Controller::PresetIndex()
{
	return _presetIndex;
}

bool Controller::MenuActive()
{
	return _menu.Active();
}

void Controller::ReloadGlobal()
{
	Storage::LoadGlobal(_global);
	ShowStatusLine();
	RefreshLeds();
}

void Controller::FactoryReset()
{
	Display::PrintLine(0, "Factory reset");
	Display::PrintLine(1, "Init memory...");
	Log("Factory reset");
	Storage::Format();

	memset(_on, 0, sizeof(_on));
	Storage::LoadGlobal(_global);
	LoadPreset(0);
}

const SwitchConfig & Controller::ConfigOf(uint8_t index)
{
	// external switches only exist in the global configuration
	if (index >= NUM_FOOT_SWITCHES || (_global.globalMask & (1UL << index)))
		return _global.switches[index];

	return _preset.switches[index];
}

void Controller::LoadPreset(uint8_t index)
{
	_presetIndex = index;
	Storage::LoadPreset(index, _preset);
	_presetSavePending = true;
	_presetChanged = millis();

	// global switches keep their state over a preset change
	for (uint8_t i = 0; i < NUM_FOOT_SWITCHES; i++)
	{
		if (!(_global.globalMask & (1UL << i)))
			_on[i] = _preset.switches[i].initialOn;
	}

	for (uint8_t i = 0; i < START_MESSAGES; i++)
		Send(_preset.start[i], true);

	_flashActive = false;
	ShowPresetLine();
	ShowStatusLine();
	RefreshLeds();
}

void Controller::OnSwitch(uint8_t index, bool pressed)
{
	const SwitchConfig & config = ConfigOf(index);

	switch (config.function)
	{
		case FN_PRESET_PREV:
			if (pressed)
				LoadPreset((_presetIndex + NUM_PRESETS - 1) % NUM_PRESETS);
			break;

		case FN_PRESET_NEXT:
			if (pressed)
				LoadPreset((_presetIndex + 1) % NUM_PRESETS);
			break;

		case FN_MIDI:
			switch (config.mode)
			{
				case MODE_TOGGLE:
					if (pressed)
					{
						_on[index] = !_on[index];
						SendMessages(config, _on[index]);
					}
					break;

				case MODE_MOMENTARY:
					_on[index] = pressed;
					SendMessages(config, pressed);
					break;

				case MODE_GROUP:
					if (pressed)
					{
						for (uint8_t i = 0; i < NUM_SWITCHES; i++)
						{
							const SwitchConfig & other = ConfigOf(i);
							if (other.function == FN_MIDI && other.mode == MODE_GROUP)
								_on[i] = false;
						}

						_on[index] = true;
						SendMessages(config, true);
					}
					break;
			}
			break;
	}

	RefreshLeds();
}

// sends all messages of a switch and shows the first one on the display
void Controller::SendMessages(const SwitchConfig & config, bool on)
{
	char text[16] = "";
	char line[LCD_COLS + 1];
	uint8_t sent = 0;

	for (uint8_t i = 0; i < MESSAGES_PER_SWITCH; i++)
	{
		if (Send(config.messages[i], on))
		{
			if (sent == 0)
				FormatMessage(config.messages[i], on, text, sizeof(text));
			sent++;
		}
	}

	if (sent == 0)
		return;

	snprintf(line, sizeof(line), "%.5s %s", config.name, text);
	if (sent > 1 && strlen(line) < LCD_COLS)
		strcat(line, "+");

	Display::PrintLine(1, line);
	Log(line);
	_flashActive = true;
	_flashStart = millis();
}

// returns true if a message was sent, a program change has no off state
bool Controller::Send(const Message & message, bool on)
{
	// a message without port selection goes to both outputs
	uint8_t port = message.port ? message.port : (uint8_t)PORT_BOTH;

	switch (message.type)
	{
		case MSG_PC:
			if (!on)
				return false;
			_midi.SendProgramChange(port, message.channel, message.number);
			return true;

		case MSG_CC:
			_midi.SendControlChange(port, message.channel, message.number, on ? message.valueOn : message.valueOff);
			return true;
	}

	return false;
}

// left led: dimmed function color, right led: state
void Controller::RefreshLeds()
{
	if (_menu.Active())
	{
		_menu.RefreshLeds();
		return;
	}

	for (uint8_t i = 0; i < NUM_FOOT_SWITCHES; i++)
	{
		const SwitchConfig & config = ConfigOf(i);

		if (config.function == FN_NONE)
		{
			_leds.SetSwitch(i, CRGB::Black, CRGB::Black);
			continue;
		}

		CRGB color = Leds::ColorOf(config.color);
		CRGB dimmed = color;
		dimmed.nscale8(LED_DIM_SCALE);

		// internal functions have no state, they light up while pressed
		bool active = (config.function == FN_MIDI) ? _on[i] : _pressed[i];

		_leds.SetSwitch(i, dimmed, active ? color : CRGB(CRGB::Black));
	}
}

void Controller::ShowPresetLine()
{
	char line[LCD_COLS + 1];

	// the preset number is shown inverted in the top left corner
	Display::LoadPresetChar(_presetIndex + 1);
	snprintf(line, sizeof(line), "%c %.12s", LCD_CHAR_PRESET, _preset.name);
	Display::PrintLine(0, line);

	snprintf(line, sizeof(line), "%u:%.12s", _presetIndex + 1, _preset.name);
	Log(line);

	Serial.print(F("!cur "));
	Serial.println(_presetIndex);
}

// shows the active switch of the group
void Controller::ShowStatusLine()
{
	char line[LCD_COLS + 1] = "";

	for (uint8_t i = 0; i < NUM_SWITCHES; i++)
	{
		const SwitchConfig & config = ConfigOf(i);

		if (config.function == FN_MIDI && config.mode == MODE_GROUP && _on[i])
		{
			snprintf(line, sizeof(line), "> %.5s", config.name);
			break;
		}
	}

	Display::PrintLine(1, line);
}
