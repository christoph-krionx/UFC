#include "Protocol.h"
#include "Controller.h"
#include "Storage.h"

#define PROTOCOL_VERSION	1

static int8_t HexValue(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

static void PrintHex(uint8_t value)
{
	static const char digits[] = "0123456789ABCDEF";

	Serial.write(digits[value >> 4]);
	Serial.write(digits[value & 0x0F]);
}

// returns the preset number of the argument or -1
static int8_t PresetArgument(const char * argument)
{
	if (argument == NULL || argument[0] < '0' || argument[0] > '9')
		return -1;

	int value = atoi(argument);
	return (value < NUM_PRESETS) ? value : -1;
}

void Protocol::Init(Controller * controller)
{
	_controller = controller;
	_headLen = 0;
	_dataLen = 0;
	_inData = false;
	_highNibble = true;
	_error = false;
}

// the command head is collected as text, the hex data after the ':' is decoded on the fly
void Protocol::Update()
{
	while (Serial.available())
	{
		char c = Serial.read();

		if (c == '\r')
			continue;

		if (c == '\n')
		{
			_head[_headLen] = 0;

			if (_headLen > 0 || _inData)
				Execute();

			_headLen = 0;
			_dataLen = 0;
			_inData = false;
			_highNibble = true;
			_error = false;
		}
		else if (_inData)
		{
			int8_t value = HexValue(c);

			if (value < 0 || _dataLen >= sizeof(_data))
			{
				_error = true;
			}
			else if (_highNibble)
			{
				_nibble = value;
				_highNibble = false;
			}
			else
			{
				_data[_dataLen++] = (_nibble << 4) | value;
				_highNibble = true;
			}
		}
		else if (c == ':')
		{
			_inData = true;
		}
		else if (_headLen < sizeof(_head) - 1)
		{
			_head[_headLen++] = c;
		}
		else
		{
			_error = true;
		}
	}
}

void Protocol::Execute()
{
	if (_error || !_highNibble)
	{
		Serial.println(F("!err syntax"));
		return;
	}

	const char * command = strtok(_head, " ");
	const char * target = strtok(NULL, " ");
	const char * argument = strtok(NULL, " ");

	if (command == NULL)
	{
		Serial.println(F("!err syntax"));
	}
	else if (strcmp(command, "info") == 0)
	{
		Serial.print(F("!info ver=" VERSION " proto="));
		Serial.print(PROTOCOL_VERSION);
		Serial.print(F(" presets="));
		Serial.print(NUM_PRESETS);
		Serial.print(F(" foot="));
		Serial.print(NUM_FOOT_SWITCHES);
		Serial.print(F(" ext="));
		Serial.print(NUM_EXT_SWITCHES);
		Serial.print(F(" msgs="));
		Serial.print(MESSAGES_PER_SWITCH);
		Serial.print(F(" start="));
		Serial.print(START_MESSAGES);
		Serial.print(F(" plen="));
		Serial.print(PRESET_NAME_LEN);
		Serial.print(F(" slen="));
		Serial.print(SWITCH_NAME_LEN);
		Serial.print(F(" gsize="));
		Serial.print(sizeof(GlobalConfig));
		Serial.print(F(" psize="));
		Serial.print(sizeof(Preset));
		Serial.print(F(" cur="));
		Serial.println(_controller->PresetIndex());
	}
	else if (strcmp(command, "get") == 0 && target != NULL && target[0] == 'g')
	{
		// the block is read into the receive buffer, there is no pending data at this point
		Storage::LoadGlobal(*(GlobalConfig *)_data);
		Serial.print(F("!g:"));
		SendBlock(_data, sizeof(GlobalConfig));
	}
	else if (strcmp(command, "get") == 0 && target != NULL && target[0] == 'p')
	{
		int8_t index = PresetArgument(argument);

		if (index < 0)
		{
			Serial.println(F("!err preset"));
			return;
		}

		Storage::LoadPreset(index, *(Preset *)_data);
		Serial.print(F("!p "));
		Serial.print(index);
		Serial.print(':');
		SendBlock(_data, sizeof(Preset));
	}
	else if (_controller->MenuActive() && (strcmp(command, "set") == 0 || strcmp(command, "load") == 0 || strcmp(command, "reset") == 0))
	{
		// the edit mode on the device works on the same data
		Serial.println(F("!err busy"));
	}
	else if (strcmp(command, "set") == 0 && target != NULL && target[0] == 'g')
	{
		if (!CheckBlock(sizeof(GlobalConfig)))
			return;

		Storage::SaveGlobal(*(const GlobalConfig *)_data);
		_controller->ReloadGlobal();
		Serial.println(F("!ok"));
	}
	else if (strcmp(command, "set") == 0 && target != NULL && target[0] == 'p')
	{
		int8_t index = PresetArgument(argument);

		if (index < 0)
		{
			Serial.println(F("!err preset"));
			return;
		}

		if (!CheckBlock(sizeof(Preset)))
			return;

		Storage::SavePreset(index, *(const Preset *)_data);

		// apply the changes immediately if the preset is in use
		if (index == _controller->PresetIndex())
			_controller->LoadPreset(index);

		Serial.println(F("!ok"));
	}
	else if (strcmp(command, "load") == 0)
	{
		int8_t index = PresetArgument(target);

		if (index < 0)
		{
			Serial.println(F("!err preset"));
			return;
		}

		_controller->LoadPreset(index);
		Serial.println(F("!ok"));
	}
	else if (strcmp(command, "leds") == 0)
	{
		const CRGB * leds = _controller->LedData();

		Serial.print(F("!leds:"));
		for (uint8_t i = 0; i < NUM_LEDS; i++)
		{
			PrintHex(leds[i].r);
			PrintHex(leds[i].g);
			PrintHex(leds[i].b);
		}
		Serial.println();
	}
	else if (strcmp(command, "reset") == 0)
	{
		_controller->FactoryReset();
		Serial.println(F("!ok"));
	}
	else
	{
		Serial.println(F("!err command"));
	}
}

void Protocol::SendBlock(const void * data, uint16_t size)
{
	const uint8_t * bytes = (const uint8_t *)data;
	uint16_t crc = Storage::Crc(bytes, size);

	for (uint16_t i = 0; i < size; i++)
		PrintHex(bytes[i]);

	PrintHex(crc & 0xFF);
	PrintHex(crc >> 8);
	Serial.println();
}

// checks size and crc of the received block
bool Protocol::CheckBlock(uint16_t size)
{
	if (_dataLen != size + 2)
	{
		Serial.println(F("!err size"));
		return false;
	}

	uint16_t crc = _data[size] | ((uint16_t)_data[size + 1] << 8);

	if (crc != Storage::Crc(_data, size))
	{
		Serial.println(F("!err crc"));
		return false;
	}

	return true;
}
