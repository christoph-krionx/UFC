/*
	UFC board with only 6 foot switches (two rows of three, one led each) and without lcd.
	Midi 2 has no input. Included by Board.h.
*/

#ifndef board_stomp6_h
#define board_stomp6_h

#define BOARD_NAME	"stomp6"
#define HAS_DISPLAY	0

#define NUM_LEDS	6

#define NUM_FOOT_SWITCHES	6
#define NUM_EXT_SWITCHES	0

// switches seen from the player: 4 5 6 in the top row, 1 2 3 in the bottom row,
// the led chain starts at switch 3 (bottom right), runs to the left and back through the top row
static const SwitchDef SWITCHES[NUM_FOOT_SWITCHES] =
{
	// bottom row, switch 1 - 3
	{ 44, 2, NO_LED }, { 45, 1, NO_LED }, { 49, 0, NO_LED },
	// top row, switch 4 - 6
	{ 35, 3, NO_LED }, { 33, 4, NO_LED }, { 41, 5, NO_LED }
};

// switches (index, 0 based) held during power on to restore the factory configuration
#define RESET_SWITCH_A	3
#define RESET_SWITCH_B	5

// layout of the configuration, has to fit into the eeprom (see Storage.cpp)
// page 1: scenes, page 2: pedalboard, a scene can set the state of every pedalboard switch
#define NUM_PRESETS		8
#define NUM_PAGES		2
#define MESSAGES_PER_SWITCH	5
#define LINK_TARGETS		NUM_FOOT_SWITCHES

#endif
