#include "Factory.h"

#define FACTORY_CHANNEL		0
#define FACTORY_FX_FIRST_CC	20
#define FACTORY_TUNER_CC	15
#define SOUNDS_PER_PRESET	6
#define NUM_FX_SWITCHES		12

static void SetName(char * dest, uint8_t size, const char * name)
{
	uint8_t len = strlen(name);

	for (uint8_t i = 0; i < size; i++)
		dest[i] = (i < len) ? name[i] : ' ';
}

static void SetMessage(Message & message, MessageType type, uint8_t number, uint8_t valueOn, uint8_t valueOff)
{
	message.type = type;
	message.port = PORT_BOTH;
	message.channel = FACTORY_CHANNEL;
	message.number = number;
	message.valueOn = valueOn;
	message.valueOff = valueOff;
}

// top row and external switches are global: preset down / up and tuner
void Factory::LoadGlobal(GlobalConfig & global)
{
	memset(&global, 0, sizeof(global));

	for (uint8_t i = 0; i < NUM_SWITCHES; i++)
	{
		SetName(global.switches[i].name, SWITCH_NAME_LEN, "");

		if (i >= 18)
			global.globalMask |= (1UL << i);
	}

	SwitchConfig & presetPrev = global.switches[18];
	SetName(presetPrev.name, SWITCH_NAME_LEN, "PRE -");
	presetPrev.function = FN_PRESET_PREV;
	presetPrev.color = COLOR_WHITE;

	SwitchConfig & presetNext = global.switches[19];
	SetName(presetNext.name, SWITCH_NAME_LEN, "PRE +");
	presetNext.function = FN_PRESET_NEXT;
	presetNext.color = COLOR_WHITE;

	SwitchConfig & tuner = global.switches[20];
	SetName(tuner.name, SWITCH_NAME_LEN, "TUNER");
	tuner.function = FN_MIDI;
	tuner.mode = MODE_TOGGLE;
	tuner.color = COLOR_RED;
	SetMessage(tuner.messages[0], MSG_CC, FACTORY_TUNER_CC, 127, 0);
}

// only the first preset has an assignment, all others are empty
// bottom row: six sounds (program change, one active), row 2 and 3: twelve effect toggles (control change)
void Factory::LoadPreset(uint8_t index, Preset & preset)
{
	char name[PRESET_NAME_LEN + 1];

	memset(&preset, 0, sizeof(preset));

	snprintf(name, sizeof(name), "PRESET %u", index + 1);
	SetName(preset.name, PRESET_NAME_LEN, name);

	for (uint8_t i = 0; i < NUM_FOOT_SWITCHES; i++)
		SetName(preset.switches[i].name, SWITCH_NAME_LEN, "");

	if (index != 0)
		return;

	for (uint8_t i = 0; i < SOUNDS_PER_PRESET; i++)
	{
		SwitchConfig & sound = preset.switches[i];
		snprintf(name, sizeof(name), "SND %u", i + 1);
		SetName(sound.name, SWITCH_NAME_LEN, name);
		sound.function = FN_MIDI;
		sound.mode = MODE_GROUP;
		sound.initialOn = (i == 0);
		sound.color = COLOR_BLUE;
		SetMessage(sound.messages[0], MSG_PC, index * SOUNDS_PER_PRESET + i, 0, 0);
	}

	for (uint8_t i = 0; i < NUM_FX_SWITCHES; i++)
	{
		SwitchConfig & fx = preset.switches[SOUNDS_PER_PRESET + i];
		snprintf(name, sizeof(name), "FX %u", i + 1);
		SetName(fx.name, SWITCH_NAME_LEN, name);
		fx.function = FN_MIDI;
		fx.mode = MODE_TOGGLE;
		fx.color = (i < 6) ? COLOR_GREEN : COLOR_ORANGE;
		SetMessage(fx.messages[0], MSG_CC, FACTORY_FX_FIRST_CC + i, 127, 0);
	}

	// select the first sound of the preset
	SetMessage(preset.start[0], MSG_PC, index * SOUNDS_PER_PRESET, 0, 0);
}
