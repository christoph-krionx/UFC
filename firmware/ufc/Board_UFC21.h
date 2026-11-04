/*
	UFC with 21 foot switches (two leds each), 4 external switches and lcd.
	Included by Board.h.
*/

#ifndef board_ufc21_h
#define board_ufc21_h

#define BOARD_NAME	"ufc21"
#define HAS_DISPLAY	1
#define HAS_MENU	1	// edit mode on the device
// line 1: preset, line 2: status and messages
#define DISPLAY_SWITCH_NAMES	0

#define NUM_LEDS	42

#define NUM_FOOT_SWITCHES	21
#define NUM_EXT_SWITCHES	4

// all switches are active low (10k pullup + 100nF on the pcb)
// foot switches are numbered from bottom left to top right,
// the led chain starts at switch 19 (top left) and snakes down
static const SwitchDef SWITCHES[NUM_FOOT_SWITCHES + NUM_EXT_SWITCHES] =
{
	// row 1 (bottom), switch 1 - 6
	{ 53, 41, 40 }, { 46, 39, 38 }, { 43, 37, 36 }, { 38, 35, 34 }, { 36, 33, 32 }, { 34, 31, 30 },
	// row 2, switch 7 - 12
	{ 39, 18, 19 }, { 52, 20, 21 }, { 50, 22, 23 }, { 48, 24, 25 }, { 47, 26, 27 }, { 42, 28, 29 },
	// row 3, switch 13 - 18
	{ 40, 17, 16 }, { 37, 15, 14 }, { 35, 13, 12 }, { 33, 11, 10 }, { 41, 9, 8 }, { 44, 7, 6 },
	// row 4 (top), switch 19 - 21
	{ 51, 0, 1 }, { 49, 2, 3 }, { 45, 4, 5 },
	// external switches, J29 pin 2 / 3, J30 pin 2 / 3
	{ 21, NO_LED, NO_LED }, { 22, NO_LED, NO_LED }, { 23, NO_LED, NO_LED }, { 24, NO_LED, NO_LED }
};

// switches (index, 0 based) held during power on to restore the factory configuration
#define RESET_SWITCH_A	18
#define RESET_SWITCH_B	20

// edit mode: hold both switches to open it
#define MENU_SWITCH_A	18
#define MENU_SWITCH_B	19

// layout of the configuration, has to fit into the eeprom (see Storage.cpp)
#define NUM_PRESETS		8
#define NUM_PAGES		1
#define MESSAGES_PER_SWITCH	3
#define LINK_TARGETS		0	// switches whose state another switch can set

#endif
