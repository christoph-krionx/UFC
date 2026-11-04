#include "Factory.h"

#define FACTORY_CHANNEL		0

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

static void SetAction(Action & action, uint8_t switchA, uint8_t switchB, SwitchFunction function)
{
	action.switchA = switchA;
	action.switchB = switchB;
	action.function = function;
	SetName(action.name, SWITCH_NAME_LEN, "");
}

// empty global configuration without actions
static void ClearGlobal(GlobalConfig & global)
{
	memset(&global, 0, sizeof(global));

	global.clockPort = PORT_BOTH;

	for (uint8_t i = 0; i < NUM_ACTIONS; i++)
		SetAction(global.actions[i], NO_SWITCH, NO_SWITCH, FN_NONE);

	for (uint8_t page = 0; page < NUM_PAGES; page++)
		for (uint8_t i = 0; i < NUM_SWITCHES; i++)
			SetName(global.switches[page][i].name, SWITCH_NAME_LEN, "");
}

// empty preset, only the name is set
static void ClearPreset(uint8_t index, Preset & preset)
{
	char name[PRESET_NAME_LEN + 1];

	memset(&preset, 0, sizeof(preset));

	snprintf(name, sizeof(name), "PRESET %u", index + 1);
	SetName(preset.name, PRESET_NAME_LEN, name);

	for (uint8_t page = 0; page < NUM_PAGES; page++)
		for (uint8_t i = 0; i < NUM_FOOT_SWITCHES; i++)
			SetName(preset.switches[page][i].name, SWITCH_NAME_LEN, "");
}

#if BOARD == BOARD_UFC21

#define FACTORY_FX_FIRST_CC	20
#define FACTORY_TUNER_CC	15
#define SOUNDS_PER_PRESET	6
#define NUM_FX_SWITCHES		12

// top row and external switches are global: preset down / up and tuner
void Factory::LoadGlobal(GlobalConfig & global)
{
	ClearGlobal(global);

	for (uint8_t i = 18; i < NUM_SWITCHES; i++)
		global.globalMask[0] |= (1UL << i);

	SwitchConfig & presetPrev = global.switches[0][18];
	SetName(presetPrev.name, SWITCH_NAME_LEN, "PRE -");
	presetPrev.function = FN_PRESET_PREV;
	presetPrev.color = COLOR_WHITE;

	SwitchConfig & presetNext = global.switches[0][19];
	SetName(presetNext.name, SWITCH_NAME_LEN, "PRE +");
	presetNext.function = FN_PRESET_NEXT;
	presetNext.color = COLOR_WHITE;

	SwitchConfig & tuner = global.switches[0][20];
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

	ClearPreset(index, preset);

	if (index != 0)
		return;

	for (uint8_t i = 0; i < SOUNDS_PER_PRESET; i++)
	{
		SwitchConfig & sound = preset.switches[0][i];
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
		SwitchConfig & fx = preset.switches[0][SOUNDS_PER_PRESET + i];
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

#elif BOARD == BOARD_STOMP6

// Sonulab StompStation PRO: fixed control changes of the blocks (on >= 64) and the tuner
#define SSP_CC_DRIVE	1
#define SSP_CC_REVERB	5
#define SSP_CC_MOD_POST	6
#define SSP_CC_COMP	8
#define SSP_CC_DELAY	10
#define SSP_CC_TUNER	11
#define SSP_CC_WAH	14

#define PAGE_STOMPS	1

struct StompDef
{
	const char * name;
	uint8_t cc;
	uint8_t color;
};

// switch 1 - 3 bottom row, 4 - 6 top row
static const StompDef stomps[NUM_FOOT_SWITCHES] =
{
	{ "DRIVE", SSP_CC_DRIVE, COLOR_ORANGE },
	{ "COMP", SSP_CC_COMP, COLOR_YELLOW },
	{ "WAH", SSP_CC_WAH, COLOR_PURPLE },
	{ "MOD", SSP_CC_MOD_POST, COLOR_CYAN },
	{ "DELAY", SSP_CC_DELAY, COLOR_GREEN },
	{ "REVRB", SSP_CC_REVERB, COLOR_BLUE }
};

// hold switch 1: tuner, hold switch 6: other page, switch 1 + 2: preset down, switch 2 + 3: preset up,
// the active scene pressed again is tap tempo, the pedalboard is global with six block toggles
void Factory::LoadGlobal(GlobalConfig & global)
{
	ClearGlobal(global);
	global.options = OPT_TAP_ACTIVE_GROUP;

	for (uint8_t i = 0; i < NUM_FOOT_SWITCHES; i++)
	{
		SwitchConfig & stomp = global.switches[PAGE_STOMPS][i];
		SetName(stomp.name, SWITCH_NAME_LEN, stomps[i].name);
		stomp.function = FN_MIDI;
		stomp.mode = MODE_TOGGLE;
		stomp.color = stomps[i].color;
		SetMessage(stomp.messages[0], MSG_CC, stomps[i].cc, 127, 0);
		global.globalMask[PAGE_STOMPS] |= (1UL << i);
	}

	Action & tuner = global.actions[0];
	SetAction(tuner, 0, NO_SWITCH, FN_MIDI);
	tuner.color = COLOR_RED;
	SetName(tuner.name, SWITCH_NAME_LEN, "TUNER");
	SetMessage(tuner.message, MSG_CC, SSP_CC_TUNER, 127, 0);

	SetAction(global.actions[1], 5, NO_SWITCH, FN_PAGE_NEXT);
	SetAction(global.actions[2], 0, 1, FN_PRESET_PREV);
	SetAction(global.actions[3], 1, 2, FN_PRESET_NEXT);
}

// all presets are empty, the scenes are assigned in the editor
void Factory::LoadPreset(uint8_t index, Preset & preset)
{
	ClearPreset(index, preset);
}

#endif
