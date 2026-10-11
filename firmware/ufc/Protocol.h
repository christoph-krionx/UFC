#ifndef protocol_h
#define protocol_h

#include "Config.h"

class Controller;

/*
	Editor protocol on the usb serial port (115200 baud), one command per line.
	Blocks are sent as hex string: raw structure followed by its crc16, see Storage.h.

	info			-> !info ver=.. proto=.. presets=.. foot=.. ext=.. msgs=.. start=.. plen=.. slen=.. gsize=.. psize=.. cur=..
	get g			-> !g:<hex>
	get p <n>		-> !p <n>:<hex>
	set g:<hex>		-> !ok
	set p <n>:<hex>		-> !ok
	load <n>		-> !ok		select preset n (0 based)
	reset			-> !ok		restore the factory configuration
	leds			-> !leds:<hex>	rgb values of all leds as held by the firmware (diagnostics)

	Errors are answered with "!err <reason>", "busy" while the edit mode on the device is open.
	The controller sends "!cur <n>" on every preset change, lines starting with '#' are log output.
*/
class Protocol
{
	public:
		void Init(Controller * controller);
		void Update();

	private:
		void Execute();
		void SendBlock(const void * data, uint16_t size);
		bool CheckBlock(uint16_t size);

		Controller * _controller;

		char _head[16];
		uint8_t _headLen;

		// holds the largest block including its crc
		uint8_t _data[sizeof(GlobalConfig) > sizeof(Preset) ? sizeof(GlobalConfig) + 2 : sizeof(Preset) + 2];
		uint16_t _dataLen;
		uint8_t _nibble;
		bool _inData;
		bool _highNibble;
		bool _error;
};

#endif
