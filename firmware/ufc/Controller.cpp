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
	Log("UFC " VERSION " " BOARD_NAME);

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
	memset(_consumed, 0, sizeof(_consumed));
	memset(_actionOn, 0, sizeof(_actionOn));
	memset(_lastPc, 0xFF, sizeof(_lastPc));
	_page = 0;
	_flashActive = false;
	_presetSavePending = false;
	_menuHold = false;
	_menu.Init(this);

	// holding two switches of the top row during power on restores the factory configuration
	bool resetRequested = (digitalRead(SWITCHES[RESET_SWITCH_A].pin) == LOW && digitalRead(SWITCHES[RESET_SWITCH_B].pin) == LOW);

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
			else if (pressed)
				OnPress(i);
			else
				OnRelease(i);
		}
	}

	if (_menu.Active())
	{
		_menu.Update();
		_leds.Update();
		return;
	}

	CheckHold();

#if HAS_DISPLAY
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
#endif

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

uint8_t Controller::PageIndex()
{
	return _page;
}

bool Controller::MenuActive()
{
	return _menu.Active();
}

void Controller::ReloadGlobal()
{
	Storage::LoadGlobal(_global);
	memset(_actionOn, 0, sizeof(_actionOn));
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
	memset(_actionOn, 0, sizeof(_actionOn));
	memset(_lastPc, 0xFF, sizeof(_lastPc));
	_page = 0;
	Storage::LoadGlobal(_global);
	LoadPreset(0);
}

bool Controller::IsGlobal(uint8_t index)
{
	// external switches only exist in the global configuration
	return index >= NUM_FOOT_SWITCHES || (_global.globalMask & (1UL << index));
}

const SwitchConfig & Controller::ConfigOf(uint8_t page, uint8_t index)
{
	if (IsGlobal(index))
		return _global.switches[index];

	return _preset.switches[page][index];
}

bool & Controller::On(uint8_t page, uint8_t index)
{
	return _on[IsGlobal(index) ? 0 : page][index];
}

void Controller::LoadPreset(uint8_t index)
{
	_presetIndex = index;
	Storage::LoadPreset(index, _preset);
	_presetSavePending = true;
	_presetChanged = millis();

	// global switches keep their state over a preset change
	for (uint8_t page = 0; page < NUM_PAGES; page++)
	{
		for (uint8_t i = 0; i < NUM_FOOT_SWITCHES; i++)
		{
			if (!IsGlobal(i))
				_on[page][i] = _preset.switches[page][i].initialOn;
		}
	}

	for (uint8_t i = 0; i < START_MESSAGES; i++)
		Send(_preset.start[i], true);

	_flashActive = false;
	ShowPresetLine();
	ShowStatusLine();
	RefreshLeds();
}

bool Controller::HasAction(uint8_t index)
{
	for (uint8_t i = 0; i < NUM_ACTIONS; i++)
	{
		const Action & action = _global.actions[i];

		if (action.function != FN_NONE && (action.switchA == index || action.switchB == index))
			return true;
	}

	return false;
}

void Controller::OnPress(uint8_t index)
{
	_pressStart[index] = millis();
	_consumed[index] = false;

	// the second switch of a combination runs the action
	for (uint8_t i = 0; i < NUM_ACTIONS; i++)
	{
		const Action & action = _global.actions[i];

		if (action.function == FN_NONE || action.switchB == NO_SWITCH)
			continue;

		uint8_t other = (action.switchA == index) ? action.switchB : (action.switchB == index) ? action.switchA : NO_SWITCH;

		if (other < NUM_SWITCHES && _pressed[other] && !_consumed[other])
		{
			_consumed[index] = true;
			_consumed[other] = true;
			RunAction(i);
			return;
		}
	}

	// without action the switch reacts immediately
	if (!HasAction(index))
		OnSwitch(index, true);
}

void Controller::OnRelease(uint8_t index)
{
	if (!HasAction(index))
	{
		OnSwitch(index, false);
		return;
	}

	// a short press of a switch with action
	if (!_consumed[index])
	{
		OnSwitch(index, true);
		OnSwitch(index, false);
	}

	_consumed[index] = false;
}

void Controller::CheckHold()
{
	for (uint8_t i = 0; i < NUM_ACTIONS; i++)
	{
		const Action & action = _global.actions[i];
		uint8_t index = action.switchA;

		if (action.function == FN_NONE || action.switchB != NO_SWITCH || index >= NUM_SWITCHES)
			continue;

		if (_pressed[index] && !_consumed[index] && millis() - _pressStart[index] >= SWITCH_HOLD_MS)
		{
			_consumed[index] = true;
			RunAction(i);
		}
	}
}

void Controller::RunAction(uint8_t number)
{
	const Action & action = _global.actions[number];
	char line[24];

	switch (action.function)
	{
		case FN_PRESET_PREV:
			LoadPreset((_presetIndex + NUM_PRESETS - 1) % NUM_PRESETS);
			break;

		case FN_PRESET_NEXT:
			LoadPreset((_presetIndex + 1) % NUM_PRESETS);
			break;

		case FN_PAGE_NEXT:
			SetPage((_page + 1) % NUM_PAGES);
			break;

		case FN_MIDI:
			_actionOn[number] = !_actionOn[number];
			Send(action.message, _actionOn[number]);
			snprintf(line, sizeof(line), "Action %u %s", number + 1, _actionOn[number] ? "on" : "off");
			Log(line);
			break;
	}

	RefreshLeds();
}

void Controller::SetPage(uint8_t page)
{
	char line[16];

	_page = page;

	snprintf(line, sizeof(line), "Page %u", _page + 1);
	Log(line);
	Serial.print(F("!page "));
	Serial.println(_page);

	_flashActive = false;
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

		case FN_PAGE_NEXT:
			if (pressed)
				SetPage((_page + 1) % NUM_PAGES);
			break;

		case FN_MIDI:
			switch (config.mode)
			{
				case MODE_TOGGLE:
					if (pressed)
					{
						On(_page, index) = !On(_page, index);
						Activate(config, On(_page, index));
					}
					break;

				case MODE_MOMENTARY:
					On(_page, index) = pressed;
					Activate(config, pressed);
					break;

				case MODE_GROUP:
					if (pressed)
					{
						for (uint8_t i = 0; i < NUM_SWITCHES; i++)
						{
							const SwitchConfig & other = ConfigOf(i);
							if (other.function == FN_MIDI && other.mode == MODE_GROUP)
								On(_page, i) = false;
						}

						On(_page, index) = true;
						Activate(config, true);
					}
					break;
			}
			break;
	}

	RefreshLeds();
}

// the own messages first, then the states of the linked switches
void Controller::Activate(const SwitchConfig & config, bool on)
{
	SendMessages(config, on);

	if (on)
		ApplyLinks(config);
}

// sets the linked switches of the last page and sends their messages
void Controller::ApplyLinks(const SwitchConfig & config)
{
#if LINK_BYTES > 0
	const uint8_t page = NUM_PAGES - 1;

	for (uint8_t t = 0; t < LINK_TARGETS; t++)
	{
		uint8_t state = (config.links[t / 4] >> ((t % 4) * 2)) & 0x03;

		if (state != LINK_ON && state != LINK_OFF)
			continue;

		const SwitchConfig & target = ConfigOf(page, t);

		if (target.function != FN_MIDI)
			continue;

		bool on = (state == LINK_ON);
		On(page, t) = on;

		for (uint8_t i = 0; i < MESSAGES_PER_SWITCH; i++)
			Send(target.messages[i], on);
	}
#else
	(void)config;
#endif
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
			if ((_global.options & OPT_SKIP_SAME_PC) && _lastPc[message.channel] == message.number)
				return false;

			_midi.SendProgramChange(port, message.channel, message.number);
			_lastPc[message.channel] = message.number;

			// the receiver may need some time to load the program before it accepts further messages
			if (_global.pcDelay)
				delay((uint16_t)_global.pcDelay * PC_DELAY_STEP_MS);
			return true;

		case MSG_CC:
			_midi.SendControlChange(port, message.channel, message.number, on ? message.valueOn : message.valueOff);
			return true;
	}

	return false;
}

// left led: dimmed function color, right led: state (a single led shows the state color or the dimmed color)
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
		bool active = (config.function == FN_MIDI) ? On(_page, i) : _pressed[i];

		_leds.SetSwitch(i, dimmed, active ? color : CRGB(CRGB::Black));
	}

	// an active midi action (for example the tuner) lights up its switch
	for (uint8_t i = 0; i < NUM_ACTIONS; i++)
	{
		const Action & action = _global.actions[i];

		if (action.function == FN_MIDI && _actionOn[i] && action.switchA < NUM_FOOT_SWITCHES)
		{
			CRGB color = Leds::ColorOf(action.color);
			_leds.SetSwitch(action.switchA, color, color);
		}
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

		if (config.function == FN_MIDI && config.mode == MODE_GROUP && On(_page, i))
		{
			snprintf(line, sizeof(line), "> %.5s", config.name);
			break;
		}
	}

	Display::PrintLine(1, line);
}
