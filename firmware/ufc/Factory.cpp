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
}

// empty global configuration without actions
static void ClearGlobal(GlobalConfig & global)
{
	memset(&global, 0, sizeof(global));

	global.clockPort = PORT_BOTH;

	for (uint8_t i = 0; i < NUM_ACTIONS; i++)
		SetAction(global.actions[i], NO_SWITCH, NO_SWITCH, FN_NONE);

	for (uint8_t i = 0; i < NUM_SWITCHES; i++)
		SetName(global.switches[i].name, SWITCH_NAME_LEN, "");
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
		global.globalMask |= (1UL << i);

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
#define SSP_CC_MOD_POST	6
#define SSP_CC_COMP	8
#define SSP_CC_REVERB	5
#define SSP_CC_DELAY	10
#define SSP_CC_TUNER	11
#define SSP_CC_BOOST	13
#define SSP_CC_WAH	14

#define PAGE_SCENES	0
#define PAGE_STOMPS	1

// presets 1 - 6 are the main sounds (StompStation preset 01A - 02C), presets 7 and 8 stay empty
#define MAIN_SOUNDS	6

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
// the active scene pressed again is tap tempo
void Factory::LoadGlobal(GlobalConfig & global)
{
	ClearGlobal(global);
	global.options = OPT_TAP_ACTIVE_GROUP;

	Action & tuner = global.actions[0];
	SetAction(tuner, 0, NO_SWITCH, FN_MIDI);
	tuner.color = COLOR_RED;
	SetMessage(tuner.message, MSG_CC, SSP_CC_TUNER, 127, 0);

	SetAction(global.actions[1], 5, NO_SWITCH, FN_PAGE_NEXT);
	SetAction(global.actions[2], 0, 1, FN_PRESET_PREV);
	SetAction(global.actions[3], 1, 2, FN_PRESET_NEXT);
}

struct SceneDef
{
	const char * name;
	uint8_t color;
	bool boost;
	// state of the pedalboard switches: DRIVE COMP WAH MOD DELAY REVRB
	bool blocks[NUM_FOOT_SWITCHES];
};

// the first scene is the sound as stored in the StompStation preset
static const SceneDef scenes[NUM_FOOT_SWITCHES] =
{
	{ "RHY", COLOR_BLUE, false, { true, false, false, false, false, true } },
	{ "SOLO", COLOR_RED, true, { true, false, false, false, true, true } },
	{ "CLEAN", COLOR_CYAN, false, { false, true, false, true, false, true } },
	{ "LEAD", COLOR_ORANGE, true, { true, true, false, false, true, true } },
	{ "AMB", COLOR_PURPLE, false, { false, true, false, true, true, true } },
	{ "DRY", COLOR_WHITE, false, { false, false, false, false, false, false } }
};

// presets 1 - 6: one main sound each, loaded with the preset, the scenes set every pedalboard block
// pedalboard: six block toggles, the leds start with the states of the first scene
void Factory::LoadPreset(uint8_t index, Preset & preset)
{
	char name[PRESET_NAME_LEN + 1];

	ClearPreset(index, preset);

	if (index >= MAIN_SOUNDS)
		return;

	snprintf(name, sizeof(name), "SOUND %u", index + 1);
	SetName(preset.name, PRESET_NAME_LEN, name);
	SetMessage(preset.start[0], MSG_PC, index, 0, 0);

	for (uint8_t i = 0; i < NUM_FOOT_SWITCHES; i++)
	{
		SwitchConfig & scene = preset.switches[PAGE_SCENES][i];
		SetName(scene.name, SWITCH_NAME_LEN, scenes[i].name);
		scene.function = FN_MIDI;
		scene.mode = MODE_GROUP;
		scene.initialOn = (i == 0);
		scene.color = scenes[i].color;
		SetMessage(scene.messages[0], MSG_PC, index, 0, 0);
		// the boost only works if the Expr block of the StompStation preset has the volume enabled
		SetMessage(scene.messages[1], MSG_CC, SSP_CC_BOOST, scenes[i].boost ? 127 : 0, 0);

		for (uint8_t t = 0; t < LINK_TARGETS; t++)
			scene.links[t / 4] |= (scenes[i].blocks[t] ? LINK_ON : LINK_OFF) << ((t % 4) * 2);

		SwitchConfig & stomp = preset.switches[PAGE_STOMPS][i];
		SetName(stomp.name, SWITCH_NAME_LEN, stomps[i].name);
		stomp.function = FN_MIDI;
		stomp.mode = MODE_TOGGLE;
		stomp.initialOn = scenes[0].blocks[i];
		stomp.color = stomps[i].color;
		SetMessage(stomp.messages[0], MSG_CC, stomps[i].cc, 127, 0);
	}
}

#endif
