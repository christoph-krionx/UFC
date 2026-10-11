#ifndef config_h
#define config_h

#include "Board.h"

#define VERSION "0.3.0"

#define SWITCH_DEBOUNCE_MS	10
#define DISPLAY_FLASH_MS	1500
#define PRESET_SAVE_DELAY_MS	3000

// edit mode: hold both switches (index, 0 based) to open it
#define MENU_SWITCH_A		18
#define MENU_SWITCH_B		19
#define MENU_HOLD_MS		1000
#define MENU_REPEAT_DELAY_MS	500
#define MENU_REPEAT_MS		100
#define MENU_NOTICE_MS		1000

#define LED_BRIGHTNESS		60
#define LED_DIM_SCALE		40
#define LED_MAX_MILLIAMPS	250	// keeps the board usable on usb power

#define NUM_PRESETS		8
#define PRESET_NAME_LEN		12
#define SWITCH_NAME_LEN		5
#define MESSAGES_PER_SWITCH	3
#define START_MESSAGES		4

//
// data model, the structures are stored in the eeprom and transferred by the editor protocol as they are,
// every change of the layout needs a new storage version (bit fields start at the least significant bit)
//

enum MessageType : uint8_t
{
	MSG_NONE,
	MSG_PC,
	MSG_CC
};

enum MidiPortMask : uint8_t
{
	PORT_1 = 1,
	PORT_2 = 2,
	PORT_BOTH = 3
};

struct Message
{
	uint8_t type : 2;
	uint8_t port : 2;
	uint8_t channel : 4;	// 0 - 15
	uint8_t number;
	uint8_t valueOn;	// cc only
	uint8_t valueOff;	// cc only
};

enum SwitchFunction : uint8_t
{
	FN_NONE,
	FN_MIDI,
	// internal functions, only useful on global switches
	FN_PRESET_PREV,
	FN_PRESET_NEXT
};

enum SwitchMode : uint8_t
{
	MODE_TOGGLE,
	MODE_MOMENTARY,
	MODE_GROUP	// only one switch of the group is active
};

enum Color : uint8_t
{
	COLOR_OFF,
	COLOR_RED,
	COLOR_ORANGE,
	COLOR_YELLOW,
	COLOR_GREEN,
	COLOR_CYAN,
	COLOR_BLUE,
	COLOR_PURPLE,
	COLOR_WHITE
};

struct SwitchConfig
{
	char name[SWITCH_NAME_LEN];	// space padded, not terminated
	uint8_t function : 3;
	uint8_t mode : 2;
	uint8_t initialOn : 1;
	uint8_t color;
	Message messages[MESSAGES_PER_SWITCH];
};

struct Preset
{
	char name[PRESET_NAME_LEN];	// space padded, not terminated
	Message start[START_MESSAGES];	// sent when the preset is loaded
	SwitchConfig switches[NUM_FOOT_SWITCHES];
};

struct GlobalConfig
{
	uint32_t globalMask;	// bit n set: switch n uses the global definition in every preset
	SwitchConfig switches[NUM_SWITCHES];
};

#endif
