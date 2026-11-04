#ifndef config_h
#define config_h

#include "Board.h"

#define VERSION "0.7.0"

#define SWITCH_DEBOUNCE_MS	20	// time a switch state has to be stable
#define SWITCH_HOLD_MS		600	// long press for actions
#define DISPLAY_FLASH_MS	1500
// line of the short messages (preset, tap tempo, sent message)
#if DISPLAY_SWITCH_NAMES
#define DISPLAY_FLASH_ROW	0
#else
#define DISPLAY_FLASH_ROW	1
#endif
#define PRESET_SAVE_DELAY_MS	3000
#define PC_DELAY_STEP_MS	10

// tap tempo and midi clock (24 ticks per quarter note)
#define TEMPO_MIN		30
#define TEMPO_MAX		300
#define TAP_TIMEOUT_MS		2000	// a longer pause starts a new tap sequence
#define TAP_AVERAGE		4	// number of intervals averaged
#define CLOCK_TICKS		24
#define CLOCK_BLINK_TICKS	6	// the tap led is lit for the first quarter of a beat

// edit mode: the switches to open it are defined by the board
#define MENU_HOLD_MS		1000
#define MENU_REPEAT_DELAY_MS	500
#define MENU_REPEAT_MS		100
#define MENU_NOTICE_MS		1000

#define LED_BRIGHTNESS		60
#define LED_DIM_SCALE		40
#define LED_MAX_MILLIAMPS	250	// keeps the board usable on usb power
// the led chain is written again after a change, a disturbed transfer (e.g. while stepping on a switch) is repaired
#define LED_REPEAT_1_MS		150
#define LED_REPEAT_2_MS		600

// NUM_PRESETS, NUM_PAGES, MESSAGES_PER_SWITCH and LINK_TARGETS are defined by the board
#define PRESET_NAME_LEN		12
#define SWITCH_NAME_LEN		5
#define START_MESSAGES		4
#define NUM_ACTIONS		4
#define NO_SWITCH		0xFF

// two bits per link target
#define LINK_BYTES		((LINK_TARGETS * 2 + 7) / 8)

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
	// internal functions, only useful on global switches and actions
	FN_PRESET_PREV,
	FN_PRESET_NEXT,
	FN_PAGE_NEXT,
	FN_TAP
};

enum SwitchMode : uint8_t
{
	MODE_TOGGLE,
	MODE_MOMENTARY,
	MODE_GROUP	// only one switch of the group (on the same page) is active
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

// state a switch sets on a link target when it is switched on
enum LinkState : uint8_t
{
	LINK_KEEP,
	LINK_OFF,
	LINK_ON
};

struct SwitchConfig
{
	char name[SWITCH_NAME_LEN];	// space padded, not terminated
	uint8_t function : 3;
	uint8_t mode : 2;
	uint8_t initialOn : 1;
	uint8_t color;
	Message messages[MESSAGES_PER_SWITCH];
#if LINK_BYTES > 0
	// link target n (switch n on the last page) in bits 2n and 2n + 1, see LinkState
	uint8_t links[LINK_BYTES];
#endif
};

struct Preset
{
	char name[PRESET_NAME_LEN];	// space padded, not terminated
	uint16_t tempo;			// bpm of the midi clock, 0: no clock until tapped
	Message start[START_MESSAGES];	// sent when the preset is loaded
	SwitchConfig switches[NUM_PAGES][NUM_FOOT_SWITCHES];
};

// a long press of switch A (switch B = NO_SWITCH) or pressing switch A and B together
struct Action
{
	uint8_t switchA;
	uint8_t switchB;
	uint8_t function;	// see SwitchFunction, FN_MIDI toggles the message on / off
	uint8_t color;		// led of switch A while a midi action is on
	Message message;
	char name[SWITCH_NAME_LEN];	// shown on the display while a midi action is on, space padded, not terminated
};

enum Options : uint8_t
{
	OPT_SKIP_SAME_PC = 1,	// a program change equal to the last one on the channel is not sent again
	OPT_TAP_ACTIVE_GROUP = 2	// pressing the active group switch again is tap tempo
};

struct GlobalConfig
{
	uint32_t globalMask[NUM_PAGES];	// bit n set: switch n of the page uses the global definition in every preset
	uint8_t pcDelay;	// pause after a program change in PC_DELAY_STEP_MS
	uint8_t options;	// see Options
	uint8_t clockPort;	// outputs of the midi clock, see MidiPortMask
	Action actions[NUM_ACTIONS];
	SwitchConfig switches[NUM_PAGES][NUM_SWITCHES];	// external switches are always global
};

#endif
