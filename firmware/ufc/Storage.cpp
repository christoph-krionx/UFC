#include "Storage.h"
#include <avr/eeprom.h>
#include <util/crc16.h>
#include "Factory.h"

#define STORAGE_VERSION		1

#define CRC_SIZE		2
#define GLOBAL_BLOCK_SIZE	(sizeof(GlobalConfig) + CRC_SIZE)
#define PRESET_BLOCK_SIZE	(sizeof(Preset) + CRC_SIZE)

#define ADDR_HEADER		0
#define ADDR_LAST_PRESET	4
#define ADDR_GLOBAL		8
#define ADDR_PRESETS		(ADDR_GLOBAL + GLOBAL_BLOCK_SIZE)

static_assert(ADDR_PRESETS + NUM_PRESETS * PRESET_BLOCK_SIZE <= E2END + 1, "configuration does not fit into the eeprom");

static const uint8_t header[4] = { 'U', 'F', 'C', STORAGE_VERSION };

static uint16_t PresetAddress(uint8_t index)
{
	return ADDR_PRESETS + index * PRESET_BLOCK_SIZE;
}

// only changed cells are written to save eeprom write cycles
static void WriteBlock(uint16_t address, const void * data, uint16_t size)
{
	eeprom_update_block(data, (void *)address, size);
	eeprom_update_word((uint16_t *)(address + size), Storage::Crc((const uint8_t *)data, size));
}

static bool ReadBlock(uint16_t address, void * data, uint16_t size)
{
	eeprom_read_block(data, (const void *)address, size);
	return eeprom_read_word((const uint16_t *)(address + size)) == Storage::Crc((const uint8_t *)data, size);
}

bool Storage::IsValid()
{
	uint8_t stored[sizeof(header)];

	eeprom_read_block(stored, (const void *)ADDR_HEADER, sizeof(stored));
	return memcmp(stored, header, sizeof(header)) == 0;
}

void Storage::Format()
{
	// invalidate the header first, an interrupted format is repeated on the next start
	eeprom_update_byte((uint8_t *)ADDR_HEADER, 0xFF);

	{
		GlobalConfig global;
		Factory::LoadGlobal(global);
		SaveGlobal(global);
	}

	for (uint8_t i = 0; i < NUM_PRESETS; i++)
	{
		Preset preset;
		Factory::LoadPreset(i, preset);
		SavePreset(i, preset);
	}

	SaveLastPreset(0);
	eeprom_update_block(header, (void *)ADDR_HEADER, sizeof(header));
}

bool Storage::LoadGlobal(GlobalConfig & global)
{
	if (ReadBlock(ADDR_GLOBAL, &global, sizeof(global)))
		return true;

	Factory::LoadGlobal(global);
	SaveGlobal(global);
	return false;
}

void Storage::SaveGlobal(const GlobalConfig & global)
{
	WriteBlock(ADDR_GLOBAL, &global, sizeof(global));
}

bool Storage::LoadPreset(uint8_t index, Preset & preset)
{
	if (ReadBlock(PresetAddress(index), &preset, sizeof(preset)))
		return true;

	Factory::LoadPreset(index, preset);
	SavePreset(index, preset);
	return false;
}

void Storage::SavePreset(uint8_t index, const Preset & preset)
{
	WriteBlock(PresetAddress(index), &preset, sizeof(preset));
}

uint8_t Storage::LoadLastPreset()
{
	uint8_t index = eeprom_read_byte((const uint8_t *)ADDR_LAST_PRESET);
	return (index < NUM_PRESETS) ? index : 0;
}

void Storage::SaveLastPreset(uint8_t index)
{
	eeprom_update_byte((uint8_t *)ADDR_LAST_PRESET, index);
}

uint16_t Storage::Crc(const uint8_t * data, uint16_t size)
{
	uint16_t crc = 0xFFFF;

	for (uint16_t i = 0; i < size; i++)
		crc = _crc16_update(crc, data[i]);

	return crc;
}
