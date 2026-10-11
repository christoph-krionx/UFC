#include "Display.h"
#include <LiquidCrystal.h>

static LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_EN, PIN_LCD_DB4, PIN_LCD_DB5, PIN_LCD_DB6, PIN_LCD_DB7);

// 3x6 digits, shown inverted as preset number with a black border of one pixel on every side
static const uint8_t digits[10][6] PROGMEM =
{
	{ 0x7, 0x5, 0x5, 0x5, 0x5, 0x7 },
	{ 0x2, 0x6, 0x2, 0x2, 0x2, 0x7 },
	{ 0x7, 0x1, 0x1, 0x7, 0x4, 0x7 },
	{ 0x7, 0x1, 0x3, 0x1, 0x1, 0x7 },
	{ 0x5, 0x5, 0x5, 0x7, 0x1, 0x1 },
	{ 0x7, 0x4, 0x7, 0x1, 0x1, 0x7 },
	{ 0x7, 0x4, 0x7, 0x5, 0x5, 0x7 },
	{ 0x7, 0x1, 0x1, 0x2, 0x2, 0x2 },
	{ 0x7, 0x5, 0x7, 0x5, 0x5, 0x7 },
	{ 0x7, 0x5, 0x5, 0x7, 0x1, 0x7 }
};

void Display::Init()
{
	lcd.begin(LCD_COLS, LCD_ROWS);
	lcd.clear();
}

void Display::PrintLine(uint8_t row, const char * text)
{
	char buf[LCD_COLS + 1];

	snprintf(buf, sizeof(buf), "%-20s", text);
	lcd.setCursor(0, row);
	lcd.print(buf);
}

void Display::LoadPresetChar(uint8_t digit)
{
	uint8_t glyph[8];

	glyph[0] = 0x1F;
	for (uint8_t row = 0; row < 6; row++)
		glyph[row + 1] = ~(pgm_read_byte(&digits[digit % 10][row]) << 1) & 0x1F;
	glyph[7] = 0x1F;

	lcd.createChar(LCD_CHAR_PRESET, glyph);
}

void Display::ShowCursor(uint8_t col, uint8_t row)
{
	lcd.setCursor(col, row);
	lcd.cursor();
}

void Display::HideCursor()
{
	lcd.noCursor();
}
