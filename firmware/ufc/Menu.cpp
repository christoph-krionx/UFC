#include "Menu.h"
#include "Controller.h"
#include "Display.h"
#include "Storage.h"

#define KEY_FIELD_PREV	0
#define KEY_FIELD_NEXT	1
#define KEY_VALUE_DOWN	2
#define KEY_VALUE_UP	3
#define KEY_BACK	4
#define KEY_SAVE	5
#define NUM_KEYS	6

#define NAME_COLUMN	6	// behind "Name: "

// a field is stored as one byte: sub index (character or message number) in the upper nibble
#define FIELD(id, sub)	((uint8_t)(((sub) << 4) | (id)))
#define FIELD_ID(field)	((field) & 0x0F)
#define FIELD_SUB(field)	((field) >> 4)

enum FieldId : uint8_t
{
	F_PRESET,
	F_TARGET,
	F_NAME,
	F_GLOBAL,
	F_FUNCTION,
	F_COLOR,
	F_MODE,
	F_INITIAL,
	F_MSG_TYPE,
	F_MSG_PORT,
	F_MSG_CHANNEL,
	F_MSG_NUMBER,
	F_MSG_ON,
	F_MSG_OFF
};

static const char nameChars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-/#";
static const uint8_t keyColors[NUM_KEYS] = { COLOR_BLUE, COLOR_BLUE, COLOR_YELLOW, COLOR_YELLOW, COLOR_RED, COLOR_GREEN };

static const char * const functionNames[] = { "keine", "MIDI", "Preset -", "Preset +" };
static const char * const modeNames[] = { "Toggle", "Momentary", "Gruppe" };
static const char * const colorNames[] = { "aus", "Rot", "Orange", "Gelb", "Gruen", "Cyan", "Blau", "Violett", "Weiss" };
static const char * const typeNames[] = { "--", "PC", "CC" };
static const char * const portNames[] = { "1+2", "1", "2", "1+2" };

// steps through a range with wrap around
static uint8_t Wrap(uint8_t value, int8_t delta, uint8_t count)
{
	if (value >= count)
		return 0;

	return (uint8_t)((value + count + delta) % count);
}

static char NextChar(char c, int8_t delta)
{
	const char * pos = strchr(nameChars, c);
	uint8_t index = (pos != NULL && c != 0) ? (uint8_t)(pos - nameChars) : 0;

	return nameChars[Wrap(index, delta, sizeof(nameChars) - 1)];
}

static void SwitchLabel(uint8_t index, char * text, uint8_t size)
{
	if (index < NUM_FOOT_SWITCHES)
		snprintf(text, size, "SW %u", index + 1);
	else
		snprintf(text, size, "EXT %u", index - NUM_FOOT_SWITCHES + 1);
}

void Menu::Init(Controller * controller)
{
	_controller = controller;
	_active = false;
}

void Menu::Enter()
{
	_active = true;
	_dirtyPreset = false;
	_dirtyGlobal = false;
	_target = 0;
	_cursor = 0;
	_held = -1;
	_noticeActive = false;

	BuildFields();
	Draw();
	RefreshLeds();
}

bool Menu::TargetGlobal()
{
	uint8_t index = _target - 1;

	return index >= NUM_FOOT_SWITCHES || (_controller->_global.globalMask & (1UL << index));
}

SwitchConfig & Menu::TargetConfig()
{
	uint8_t index = _target - 1;

	return TargetGlobal() ? _controller->_global.switches[index] : _controller->_preset.switches[index];
}

Message & Menu::MessageOf(uint8_t number)
{
	return (_target == 0) ? _controller->_preset.start[number] : TargetConfig().messages[number];
}

void Menu::AddField(uint8_t id, uint8_t sub)
{
	if (_count < sizeof(_fields))
		_fields[_count++] = FIELD(id, sub);
}

void Menu::AddMessageFields(uint8_t number, const Message & message, bool withOff)
{
	AddField(F_MSG_TYPE, number);

	if (message.type == MSG_NONE)
		return;

	AddField(F_MSG_PORT, number);
	AddField(F_MSG_CHANNEL, number);
	AddField(F_MSG_NUMBER, number);

	if (message.type == MSG_CC)
	{
		AddField(F_MSG_ON, number);
		if (withOff)
			AddField(F_MSG_OFF, number);
	}
}

// only the fields which make sense for the current settings are offered
void Menu::BuildFields()
{
	uint8_t current = (_cursor < _count) ? _fields[_cursor] : FIELD(F_PRESET, 0);

	_count = 0;
	AddField(F_PRESET, 0);
	AddField(F_TARGET, 0);

	if (_target == 0)
	{
		for (uint8_t i = 0; i < PRESET_NAME_LEN; i++)
			AddField(F_NAME, i);

		for (uint8_t i = 0; i < START_MESSAGES; i++)
			AddMessageFields(i, _controller->_preset.start[i], false);
	}
	else
	{
		const SwitchConfig & config = TargetConfig();

		if (_target - 1 < NUM_FOOT_SWITCHES)
			AddField(F_GLOBAL, 0);

		for (uint8_t i = 0; i < SWITCH_NAME_LEN; i++)
			AddField(F_NAME, i);

		AddField(F_FUNCTION, 0);

		if (config.function != FN_NONE)
			AddField(F_COLOR, 0);

		if (config.function == FN_MIDI)
		{
			AddField(F_MODE, 0);

			if (!TargetGlobal() && config.mode != MODE_MOMENTARY)
				AddField(F_INITIAL, 0);

			for (uint8_t i = 0; i < MESSAGES_PER_SWITCH; i++)
				AddMessageFields(i, config.messages[i], true);
		}
	}

	// stay on the same field if it still exists
	for (uint8_t i = 0; i < _count; i++)
	{
		if (_fields[i] == current)
		{
			_cursor = i;
			return;
		}
	}

	if (_cursor >= _count)
		_cursor = _count - 1;
}

void Menu::OnSwitch(uint8_t index, bool pressed)
{
	if (!pressed)
	{
		if (index == _held)
			_held = -1;
		return;
	}

	if (index < NUM_KEYS)
	{
		HandleKey(index);

		if (index <= KEY_VALUE_UP)
		{
			_held = index;
			_nextRepeat = millis() + MENU_REPEAT_DELAY_MS;
		}
	}
	else
	{
		// stepping on a switch selects it for editing
		_target = index + 1;
		_cursor = 1;
		_count = 0;
		BuildFields();
		_cursor = 1;
		Draw();
		RefreshLeds();
	}
}

void Menu::Update()
{
	if (_held >= 0 && (int32_t)(millis() - _nextRepeat) >= 0)
	{
		HandleKey(_held);
		_nextRepeat = millis() + MENU_REPEAT_MS;
	}

	if (_noticeActive && millis() - _noticeStart >= MENU_NOTICE_MS)
	{
		_noticeActive = false;
		Draw();
	}
}

void Menu::HandleKey(uint8_t key)
{
	switch (key)
	{
		case KEY_FIELD_PREV:
			_cursor = Wrap(_cursor, -1, _count);
			break;

		case KEY_FIELD_NEXT:
			_cursor = Wrap(_cursor, 1, _count);
			break;

		case KEY_VALUE_DOWN:
			Adjust(-1);
			break;

		case KEY_VALUE_UP:
			Adjust(1);
			break;

		case KEY_BACK:
			Exit();
			return;

		case KEY_SAVE:
			Save();
			return;
	}

	_noticeActive = false;
	Draw();
	RefreshLeds();
}

void Menu::Adjust(int8_t delta)
{
	uint8_t id = FIELD_ID(_fields[_cursor]);
	uint8_t sub = FIELD_SUB(_fields[_cursor]);

	switch (id)
	{
		case F_PRESET:
			if (_dirtyPreset)
			{
				Notice("Erst speichern!");
				return;
			}

			_controller->_presetIndex = Wrap(_controller->_presetIndex, delta, NUM_PRESETS);
			Storage::LoadPreset(_controller->_presetIndex, _controller->_preset);
			break;

		case F_TARGET:
			_target = Wrap(_target, delta, NUM_SWITCHES + 1);
			break;

		case F_NAME:
		{
			char * name = (_target == 0) ? _controller->_preset.name : TargetConfig().name;
			name[sub] = NextChar(name[sub], delta);
			MarkDirty();
			break;
		}

		case F_GLOBAL:
			SetGlobal(_target - 1, !TargetGlobal());
			break;

		case F_FUNCTION:
		{
			// internal functions are reserved for global switches
			SwitchConfig & config = TargetConfig();
			config.function = Wrap(config.function, delta, TargetGlobal() ? 4 : 2);
			if (config.function != FN_NONE && config.color == COLOR_OFF)
				config.color = COLOR_WHITE;
			MarkDirty();
			break;
		}

		case F_COLOR:
		{
			SwitchConfig & config = TargetConfig();
			config.color = Wrap(config.color - 1, delta, COLOR_WHITE) + 1;
			MarkDirty();
			break;
		}

		case F_MODE:
		{
			SwitchConfig & config = TargetConfig();
			config.mode = Wrap(config.mode, delta, 3);
			MarkDirty();
			break;
		}

		case F_INITIAL:
		{
			SwitchConfig & config = TargetConfig();
			config.initialOn = !config.initialOn;
			MarkDirty();
			break;
		}

		case F_MSG_TYPE:
		{
			Message & message = MessageOf(sub);
			message.type = Wrap(message.type, delta, 3);
			if (message.port == 0)
				message.port = PORT_BOTH;
			if (message.type == MSG_CC && message.valueOn == 0 && message.valueOff == 0)
				message.valueOn = 127;
			MarkDirty();
			break;
		}

		case F_MSG_PORT:
		{
			Message & message = MessageOf(sub);
			message.port = Wrap(message.port - 1, delta, 3) + 1;
			MarkDirty();
			break;
		}

		case F_MSG_CHANNEL:
		{
			Message & message = MessageOf(sub);
			message.channel = Wrap(message.channel, delta, 16);
			MarkDirty();
			break;
		}

		case F_MSG_NUMBER:
			MessageOf(sub).number = Wrap(MessageOf(sub).number, delta, 128);
			MarkDirty();
			break;

		case F_MSG_ON:
			MessageOf(sub).valueOn = Wrap(MessageOf(sub).valueOn, delta, 128);
			MarkDirty();
			break;

		case F_MSG_OFF:
			MessageOf(sub).valueOff = Wrap(MessageOf(sub).valueOff, delta, 128);
			MarkDirty();
			break;
	}

	BuildFields();
}

// the definition is copied to the other side so that the switch keeps its function
void Menu::SetGlobal(uint8_t index, bool global)
{
	GlobalConfig & config = _controller->_global;
	Preset & preset = _controller->_preset;

	if (global)
	{
		config.switches[index] = preset.switches[index];
		config.switches[index].initialOn = false;
		config.globalMask |= (1UL << index);
	}
	else
	{
		preset.switches[index] = config.switches[index];
		if (preset.switches[index].function != FN_MIDI)
			preset.switches[index].function = FN_NONE;
		config.globalMask &= ~(1UL << index);
		_dirtyPreset = true;
	}

	_dirtyGlobal = true;
}

void Menu::MarkDirty()
{
	if (_target == 0 || !TargetGlobal())
		_dirtyPreset = true;
	else
		_dirtyGlobal = true;
}

void Menu::Save()
{
	if (_dirtyPreset)
		Storage::SavePreset(_controller->_presetIndex, _controller->_preset);

	if (_dirtyGlobal)
		Storage::SaveGlobal(_controller->_global);

	_dirtyPreset = false;
	_dirtyGlobal = false;
	Draw();
	Notice("Gespeichert");
}

// leaves the edit mode, the preset is loaded again from the storage
void Menu::Exit()
{
	if (_dirtyGlobal)
		Storage::LoadGlobal(_controller->_global);

	_active = false;
	_held = -1;
	Display::HideCursor();
	_controller->LoadPreset(_controller->_presetIndex);
}

void Menu::Notice(const char * text)
{
	Display::HideCursor();
	Display::PrintLine(1, text);
	_noticeActive = true;
	_noticeStart = millis();
}

void Menu::Draw()
{
	char line[LCD_COLS + 1];
	char label[8];
	uint8_t id = FIELD_ID(_fields[_cursor]);
	uint8_t sub = FIELD_SUB(_fields[_cursor]);
	char prefix = (_target == 0) ? 'S' : 'M';

	Display::HideCursor();

	// first line: what is edited, a star marks unsaved changes
	if (_target == 0)
	{
		snprintf(line, sizeof(line), "EDIT P%u Preset", _controller->_presetIndex + 1);
	}
	else
	{
		SwitchLabel(_target - 1, label, sizeof(label));

		if (TargetGlobal())
			snprintf(line, sizeof(line), "EDIT GL %s %.5s", label, TargetConfig().name);
		else
			snprintf(line, sizeof(line), "EDIT P%u %s %.5s", _controller->_presetIndex + 1, label, TargetConfig().name);
	}

	if (_dirtyPreset || _dirtyGlobal)
	{
		for (uint8_t i = strlen(line); i < LCD_COLS; i++)
			line[i] = ' ';
		line[LCD_COLS - 1] = '*';
		line[LCD_COLS] = 0;
	}

	Display::PrintLine(0, line);

	// second line: the selected field
	switch (id)
	{
		case F_PRESET:
			snprintf(line, sizeof(line), "Preset: %u %.10s", _controller->_presetIndex + 1, _controller->_preset.name);
			break;

		case F_TARGET:
			if (_target == 0)
			{
				strcpy(line, "Ziel: Preset");
			}
			else
			{
				SwitchLabel(_target - 1, label, sizeof(label));
				snprintf(line, sizeof(line), "Ziel: %s", label);
			}
			break;

		case F_NAME:
			if (_target == 0)
				snprintf(line, sizeof(line), "Name: %.12s", _controller->_preset.name);
			else
				snprintf(line, sizeof(line), "Name: %.5s", TargetConfig().name);
			break;

		case F_GLOBAL:
			snprintf(line, sizeof(line), "Global: %s", TargetGlobal() ? "ja" : "nein");
			break;

		case F_FUNCTION:
			snprintf(line, sizeof(line), "Funktion: %s", functionNames[TargetConfig().function & 0x03]);
			break;

		case F_COLOR:
			snprintf(line, sizeof(line), "Farbe: %s", colorNames[TargetConfig().color <= COLOR_WHITE ? TargetConfig().color : 0]);
			break;

		case F_MODE:
			snprintf(line, sizeof(line), "Modus: %s", modeNames[TargetConfig().mode < 3 ? TargetConfig().mode : 0]);
			break;

		case F_INITIAL:
			snprintf(line, sizeof(line), "Beim Laden: %s", TargetConfig().initialOn ? "an" : "aus");
			break;

		case F_MSG_TYPE:
			snprintf(line, sizeof(line), "%c%u Typ: %s", prefix, sub + 1, typeNames[MessageOf(sub).type < 3 ? MessageOf(sub).type : 0]);
			break;

		case F_MSG_PORT:
			snprintf(line, sizeof(line), "%c%u MIDI Out: %s", prefix, sub + 1, portNames[MessageOf(sub).port]);
			break;

		case F_MSG_CHANNEL:
			snprintf(line, sizeof(line), "%c%u Kanal: %u", prefix, sub + 1, MessageOf(sub).channel + 1);
			break;

		case F_MSG_NUMBER:
			snprintf(line, sizeof(line), "%c%u %s: %u", prefix, sub + 1, MessageOf(sub).type == MSG_PC ? "Programm" : "Controller", MessageOf(sub).number);
			break;

		case F_MSG_ON:
			snprintf(line, sizeof(line), "%c%u Wert%s: %u", prefix, sub + 1, _target == 0 ? "" : " an", MessageOf(sub).valueOn);
			break;

		case F_MSG_OFF:
			snprintf(line, sizeof(line), "%c%u Wert aus: %u", prefix, sub + 1, MessageOf(sub).valueOff);
			break;
	}

	Display::PrintLine(1, line);

	// the cursor marks the character which is changed
	if (id == F_NAME)
		Display::ShowCursor(NAME_COLUMN + sub, 1);
}

// left led of the bottom row: key colors, right led of the edited switch: its color
void Menu::RefreshLeds()
{
	for (uint8_t i = 0; i < NUM_FOOT_SWITCHES; i++)
	{
		CRGB left = CRGB::Black;
		CRGB right = CRGB::Black;

		if (i < NUM_KEYS)
		{
			left = Leds::ColorOf(keyColors[i]);
			left.nscale8(LED_DIM_SCALE);
		}

		if (_target == i + 1)
		{
			const SwitchConfig & config = TargetConfig();
			right = Leds::ColorOf(config.function != FN_NONE ? config.color : (uint8_t)COLOR_WHITE);
		}

		_controller->_leds.SetSwitch(i, left, right);
	}
}
