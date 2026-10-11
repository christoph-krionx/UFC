#ifndef storage_h
#define storage_h

#include "Config.h"

/*
	Configuration storage in the internal eeprom.

	address 0:	'U' 'F' 'C' <storage version>
	address 4:	last used preset
	address 8:	global block
	following:	NUM_PRESETS preset blocks

	A block is the raw structure followed by its crc16 (little endian),
	the serial protocol transfers the blocks in exactly this format.
*/
namespace Storage
{
	// returns false if the eeprom holds no valid configuration of this version
	bool IsValid();
	// writes the factory configuration, takes several seconds
	void Format();

	// a block with a wrong checksum is replaced by its factory default (returns false)
	bool LoadGlobal(GlobalConfig & global);
	void SaveGlobal(const GlobalConfig & global);
	bool LoadPreset(uint8_t index, Preset & preset);
	void SavePreset(uint8_t index, const Preset & preset);

	uint8_t LoadLastPreset();
	void SaveLastPreset(uint8_t index);

	// crc16 with polynomial 0xA001 and start value 0xFFFF (crc-16/modbus)
	uint16_t Crc(const uint8_t * data, uint16_t size);
}

#endif
