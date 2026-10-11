/*
	Binary format of the ufc configuration blocks, see firmware/ufc/Config.h and Storage.h.
	A block is the raw structure followed by its crc16 (little endian), transferred as hex string.
*/
(function (root) {
	'use strict';

	const MESSAGE_SIZE = 4;

	// matches the firmware defaults, replaced by the values of the info response after connecting
	const DEFAULT_LAYOUT = { presets: 8, foot: 21, ext: 4, msgs: 3, start: 4, plen: 12, slen: 5 };

	function switchSize(layout) {
		return layout.slen + 2 + layout.msgs * MESSAGE_SIZE;
	}

	function presetSize(layout) {
		return layout.plen + layout.start * MESSAGE_SIZE + layout.foot * switchSize(layout);
	}

	function globalSize(layout) {
		return 4 + (layout.foot + layout.ext) * switchSize(layout);
	}

	// crc-16/modbus: polynomial 0xA001, start value 0xFFFF
	function crc16(bytes) {
		let crc = 0xFFFF;
		for (const b of bytes) {
			crc ^= b;
			for (let i = 0; i < 8; i++)
				crc = (crc & 1) ? (crc >>> 1) ^ 0xA001 : crc >>> 1;
		}
		return crc;
	}

	function toHex(bytes) {
		let hex = '';
		for (const b of bytes)
			hex += b.toString(16).padStart(2, '0').toUpperCase();
		return hex;
	}

	function blockToHex(bytes) {
		const crc = crc16(bytes);
		return toHex(bytes) + toHex([crc & 0xFF, crc >>> 8]);
	}

	// returns the block without crc, throws if the hex string or the crc is invalid
	function hexToBlock(hex, expectedSize) {
		if (!/^([0-9a-fA-F]{2})+$/.test(hex))
			throw new Error('Ungültige Hex-Daten');

		const bytes = new Uint8Array(hex.length / 2);
		for (let i = 0; i < bytes.length; i++)
			bytes[i] = parseInt(hex.substr(i * 2, 2), 16);

		if (bytes.length !== expectedSize + 2)
			throw new Error('Blockgröße ' + (bytes.length - 2) + ' statt ' + expectedSize);

		const data = bytes.subarray(0, expectedSize);
		if (crc16(data) !== (bytes[expectedSize] | (bytes[expectedSize + 1] << 8)))
			throw new Error('Prüfsumme falsch');

		return data;
	}

	function readName(bytes, offset, length) {
		let name = '';
		for (let i = 0; i < length; i++) {
			const c = bytes[offset + i];
			name += (c >= 0x20 && c <= 0x7E) ? String.fromCharCode(c) : ' ';
		}
		return name.replace(/ +$/, '');
	}

	// names are space padded and not terminated
	function writeName(bytes, offset, length, name) {
		for (let i = 0; i < length; i++) {
			const c = i < name.length ? name.charCodeAt(i) : 0x20;
			bytes[offset + i] = (c >= 0x20 && c <= 0x7E) ? c : 0x20;
		}
	}

	function readMessage(bytes, offset) {
		return {
			type: bytes[offset] & 0x03,
			port: (bytes[offset] >> 2) & 0x03,
			channel: (bytes[offset] >> 4) & 0x0F,
			number: bytes[offset + 1],
			valueOn: bytes[offset + 2],
			valueOff: bytes[offset + 3]
		};
	}

	function writeMessage(bytes, offset, message) {
		bytes[offset] = (message.type & 0x03) | ((message.port & 0x03) << 2) | ((message.channel & 0x0F) << 4);
		bytes[offset + 1] = message.number & 0x7F;
		bytes[offset + 2] = message.valueOn & 0x7F;
		bytes[offset + 3] = message.valueOff & 0x7F;
	}

	function readSwitch(layout, bytes, offset) {
		const flags = bytes[offset + layout.slen];
		const config = {
			name: readName(bytes, offset, layout.slen),
			fn: flags & 0x07,
			mode: (flags >> 3) & 0x03,
			initialOn: ((flags >> 5) & 0x01) === 1,
			color: bytes[offset + layout.slen + 1],
			messages: []
		};

		for (let i = 0; i < layout.msgs; i++)
			config.messages.push(readMessage(bytes, offset + layout.slen + 2 + i * MESSAGE_SIZE));

		return config;
	}

	function writeSwitch(layout, bytes, offset, config) {
		writeName(bytes, offset, layout.slen, config.name);
		bytes[offset + layout.slen] = (config.fn & 0x07) | ((config.mode & 0x03) << 3) | (config.initialOn ? 0x20 : 0);
		bytes[offset + layout.slen + 1] = config.color & 0xFF;

		for (let i = 0; i < layout.msgs; i++)
			writeMessage(bytes, offset + layout.slen + 2 + i * MESSAGE_SIZE, config.messages[i]);
	}

	function decodePreset(layout, bytes) {
		const preset = { name: readName(bytes, 0, layout.plen), start: [], switches: [] };
		let offset = layout.plen;

		for (let i = 0; i < layout.start; i++, offset += MESSAGE_SIZE)
			preset.start.push(readMessage(bytes, offset));

		for (let i = 0; i < layout.foot; i++, offset += switchSize(layout))
			preset.switches.push(readSwitch(layout, bytes, offset));

		return preset;
	}

	function encodePreset(layout, preset) {
		const bytes = new Uint8Array(presetSize(layout));
		let offset = layout.plen;

		writeName(bytes, 0, layout.plen, preset.name);

		for (let i = 0; i < layout.start; i++, offset += MESSAGE_SIZE)
			writeMessage(bytes, offset, preset.start[i]);

		for (let i = 0; i < layout.foot; i++, offset += switchSize(layout))
			writeSwitch(layout, bytes, offset, preset.switches[i]);

		return bytes;
	}

	function decodeGlobal(layout, bytes) {
		const global = {
			mask: (bytes[0] | (bytes[1] << 8) | (bytes[2] << 16) | (bytes[3] << 24)) >>> 0,
			switches: []
		};

		for (let i = 0, offset = 4; i < layout.foot + layout.ext; i++, offset += switchSize(layout))
			global.switches.push(readSwitch(layout, bytes, offset));

		return global;
	}

	function encodeGlobal(layout, global) {
		const bytes = new Uint8Array(globalSize(layout));

		for (let i = 0; i < 4; i++)
			bytes[i] = (global.mask >>> (i * 8)) & 0xFF;

		for (let i = 0, offset = 4; i < layout.foot + layout.ext; i++, offset += switchSize(layout))
			writeSwitch(layout, bytes, offset, global.switches[i]);

		return bytes;
	}

	const api = {
		DEFAULT_LAYOUT, presetSize, globalSize, crc16, blockToHex, hexToBlock,
		decodePreset, encodePreset, decodeGlobal, encodeGlobal
	};

	if (typeof module !== 'undefined' && module.exports)
		module.exports = api;
	else
		root.UfcCodec = api;
})(typeof self !== 'undefined' ? self : this);
