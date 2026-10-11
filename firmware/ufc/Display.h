#ifndef display_h
#define display_h

#include "Config.h"

// custom character holding the inverted preset number
#define LCD_CHAR_PRESET	1

namespace Display
{
	void Init();
	// writes a complete line, the rest of the line is cleared
	void PrintLine(uint8_t row, const char * text);
	// loads the inverted digit (black block with the digit left blank) as custom character
	void LoadPresetChar(uint8_t digit);
	void ShowCursor(uint8_t col, uint8_t row);
	void HideCursor();
}

#endif
