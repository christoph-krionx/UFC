/*
	Binary format of the ufc configuration blocks, see firmware/ufc/Config.h and Storage.h.
	A block is the raw structure followed by its crc16 (little endian), transferred as hex string.
*/
(function (root) {
	'use strict';

	const MESSAGE_SIZE = 4;
	const NO_SWITCH = 0xFF;

	// matches the firmware defaults (ufc21), replaced by the values of the info response after connecting
	const DEFAULT_LAYOUT = { display: 1, presets: 8, pages: 1, foot: 21, ext: 4, msgs: 3, links: 0, actions: 4, start: 4, plen: 12, slen: 5 };

	// two bits per link target
	function linkBytes(layout) {
		return Math.ceil(layout.links * 2 / 8);
	}

	function switchSize(layout) {
		return layout.slen + 2 + layout.msgs * MESSAGE_SIZE + linkBytes(layout);
	}

	// switches, function, color, message, name
	function actionSize(layout) {
		return 4 + MESSAGE_SIZE + layout.slen;
	}

	function presetSize(layout) {
		return layout.plen + 2 + layout.start * MESSAGE_SIZE + layout.pages * layout.foot * switchSize(layout);
	}

	function globalSize(layout) {
		return layout.pages * 4 + 3 + layout.actions * actionSize(layout) + layout.pages * (layout.foot + layout.ext) * switchSize(layout);
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

		// link state of target n: 0 unchanged, 1 off, 2 on
		const links = offset + layout.slen + 2 + layout.msgs * MESSAGE_SIZE;
		config.links = [];
		for (let i = 0; i < layout.links; i++)
			config.links.push((bytes[links + (i >> 2)] >> ((i & 3) * 2)) & 0x03);

		return config;
	}

	function writeSwitch(layout, bytes, offset, config) {
		writeName(bytes, offset, layout.slen, config.name);
		bytes[offset + layout.slen] = (config.fn & 0x07) | ((config.mode & 0x03) << 3) | (config.initialOn ? 0x20 : 0);
		bytes[offset + layout.slen + 1] = config.color & 0xFF;

		for (let i = 0; i < layout.msgs; i++)
			writeMessage(bytes, offset + layout.slen + 2 + i * MESSAGE_SIZE, config.messages[i]);

		const links = offset + layout.slen + 2 + layout.msgs * MESSAGE_SIZE;
		for (let i = 0; i < linkBytes(layout); i++)
			bytes[links + i] = 0;
		for (let i = 0; i < layout.links; i++)
			bytes[links + (i >> 2)] |= ((config.links[i] || 0) & 0x03) << ((i & 3) * 2);
	}

	// the switches of a preset are stored page by page
	function decodePreset(layout, bytes) {
		const preset = { name: readName(bytes, 0, layout.plen), tempo: bytes[layout.plen] | (bytes[layout.plen + 1] << 8), start: [], pages: [] };
		let offset = layout.plen + 2;

		for (let i = 0; i < layout.start; i++, offset += MESSAGE_SIZE)
			preset.start.push(readMessage(bytes, offset));

		for (let page = 0; page < layout.pages; page++) {
			const switches = [];
			for (let i = 0; i < layout.foot; i++, offset += switchSize(layout))
				switches.push(readSwitch(layout, bytes, offset));
			preset.pages.push(switches);
		}

		return preset;
	}

	function encodePreset(layout, preset) {
		const bytes = new Uint8Array(presetSize(layout));
		let offset = layout.plen + 2;

		writeName(bytes, 0, layout.plen, preset.name);
		bytes[layout.plen] = preset.tempo & 0xFF;
		bytes[layout.plen + 1] = (preset.tempo >> 8) & 0xFF;

		for (let i = 0; i < layout.start; i++, offset += MESSAGE_SIZE)
			writeMessage(bytes, offset, preset.start[i]);

		for (let page = 0; page < layout.pages; page++)
			for (let i = 0; i < layout.foot; i++, offset += switchSize(layout))
				writeSwitch(layout, bytes, offset, preset.pages[page][i]);

		return bytes;
	}

	function decodeGlobal(layout, bytes) {
		// one mask and one set of global switches per page
		const global = { masks: [], actions: [], switches: [] };
		let offset = 0;

		for (let page = 0; page < layout.pages; page++, offset += 4)
			global.masks.push((bytes[offset] | (bytes[offset + 1] << 8) | (bytes[offset + 2] << 16) | (bytes[offset + 3] << 24)) >>> 0);

		global.pcDelay = bytes[offset];
		global.options = bytes[offset + 1];
		global.clockPort = bytes[offset + 2];
		offset += 3;

		for (let i = 0; i < layout.actions; i++, offset += actionSize(layout)) {
			global.actions.push({
				a: bytes[offset],
				b: bytes[offset + 1],
				fn: bytes[offset + 2],
				color: bytes[offset + 3],
				message: readMessage(bytes, offset + 4),
				name: readName(bytes, offset + 4 + MESSAGE_SIZE, layout.slen)
			});
		}

		for (let page = 0; page < layout.pages; page++) {
			const switches = [];
			for (let i = 0; i < layout.foot + layout.ext; i++, offset += switchSize(layout))
				switches.push(readSwitch(layout, bytes, offset));
			global.switches.push(switches);
		}

		return global;
	}

	function encodeGlobal(layout, global) {
		const bytes = new Uint8Array(globalSize(layout));

		let offset = 0;

		for (let page = 0; page < layout.pages; page++, offset += 4)
			for (let i = 0; i < 4; i++)
				bytes[offset + i] = (global.masks[page] >>> (i * 8)) & 0xFF;

		bytes[offset] = global.pcDelay & 0xFF;
		bytes[offset + 1] = global.options & 0xFF;
		bytes[offset + 2] = global.clockPort & 0xFF;
		offset += 3;

		for (let i = 0; i < layout.actions; i++, offset += actionSize(layout)) {
			const action = global.actions[i];
			bytes[offset] = action.a & 0xFF;
			bytes[offset + 1] = action.b & 0xFF;
			bytes[offset + 2] = action.fn & 0xFF;
			bytes[offset + 3] = action.color & 0xFF;
			writeMessage(bytes, offset + 4, action.message);
			writeName(bytes, offset + 4 + MESSAGE_SIZE, layout.slen, action.name || '');
		}

		for (let page = 0; page < layout.pages; page++)
			for (let i = 0; i < layout.foot + layout.ext; i++, offset += switchSize(layout))
				writeSwitch(layout, bytes, offset, global.switches[page][i]);

		return bytes;
	}

	//
	// configuration files of older editor versions
	//

	function emptyMessage() {
		return { type: 0, port: 0, channel: 0, number: 0, valueOn: 0, valueOff: 0 };
	}

	function emptySwitch(layout) {
		return { name: '', fn: 0, mode: 0, initialOn: false, color: 0, messages: [], links: [] };
	}

	function clone(value) {
		return JSON.parse(JSON.stringify(value));
	}

	// fills up or cuts a list to the given length
	function fit(list, length, create) {
		const result = Array.isArray(list) ? list.slice(0, length) : [];
		while (result.length < length)
			result.push(create());
		return result;
	}

	function fixSwitch(layout, config) {
		const result = Object.assign(emptySwitch(layout), config || {});
		result.messages = fit(result.messages, layout.msgs, emptyMessage);
		result.links = fit(result.links, layout.links, () => 0);
		return result;
	}

	function fixSwitches(layout, switches, count) {
		return fit(switches, count, () => null).map((config) => fixSwitch(layout, config));
	}

	/*
		Brings a configuration file of any editor version into the current format.
		File version 1 (firmware 0.3): one set of switches per preset, one global mask, no actions.
		File version 2 (firmware 0.4 / 0.5): pages per preset, one global mask for all pages.
		File version 3 (firmware 0.6): one global mask and one set of global switches per page.
		File version 4 (firmware 0.7): actions have a name.
		Values the file does not have are taken from base (the configuration of the connected device) or set to defaults.
	*/
	function migrate(data, layout, base) {
		if (!data || data.format !== 'ufc-config' || !data.global || !Array.isArray(data.presets))
			throw new Error('Das ist keine UFC-Konfigurationsdatei');

		const old = data.layout || {};
		if (old.foot !== layout.foot || (old.ext || 0) !== layout.ext)
			throw new Error('Die Datei gehört zu einem Gerät mit ' + old.foot + ' statt ' + layout.foot + ' Tastern');

		const count = layout.foot + layout.ext;
		const g = data.global;
		const global = {};

		// before version 3 the global switches and the mask were valid on every page
		if (Array.isArray(g.masks))
			global.masks = fit(g.masks, layout.pages, () => 0);
		else
			global.masks = fit([], layout.pages, () => (g.mask || 0) >>> 0);

		if (Array.isArray(g.switches) && Array.isArray(g.switches[0]))
			global.switches = fit(g.switches, layout.pages, () => []).map((switches) => fixSwitches(layout, switches, count));
		else
			global.switches = fit([], layout.pages, () => fixSwitches(layout, clone(g.switches || []), count));

		global.pcDelay = g.pcDelay !== undefined ? g.pcDelay : (base ? base.pcDelay : 0);
		global.options = g.options !== undefined ? g.options : (base ? base.options : 0);
		global.clockPort = g.clockPort !== undefined ? g.clockPort : (base ? base.clockPort : 3);

		const noAction = () => ({ a: NO_SWITCH, b: NO_SWITCH, fn: 0, color: 0, message: emptyMessage(), name: '' });
		const actions = Array.isArray(g.actions) ? g.actions : (base ? clone(base.actions) : []);
		global.actions = fit(actions, layout.actions, noAction).map((action) => {
			const result = Object.assign(noAction(), action);
			result.message = Object.assign(emptyMessage(), result.message);
			return result;
		});

		const presets = [];
		for (let i = 0; i < layout.presets; i++) {
			const p = data.presets[i] || { name: 'PRESET ' + (i + 1) };
			const pages = Array.isArray(p.pages) ? p.pages : (Array.isArray(p.switches) ? [p.switches] : []);

			presets.push({
				name: p.name || '',
				tempo: p.tempo || 0,
				start: fit(p.start, layout.start, emptyMessage).map((message) => Object.assign(emptyMessage(), message)),
				pages: fit(pages, layout.pages, () => []).map((switches) => fixSwitches(layout, switches, layout.foot))
			});
		}

		// encoding and decoding again brings every value into its valid range
		return {
			global: decodeGlobal(layout, encodeGlobal(layout, global)),
			presets: presets.map((preset) => decodePreset(layout, encodePreset(layout, preset)))
		};
	}

	const api = {
		DEFAULT_LAYOUT, NO_SWITCH, presetSize, globalSize, crc16, blockToHex, hexToBlock,
		decodePreset, encodePreset, decodeGlobal, encodeGlobal, migrate
	};

	if (typeof module !== 'undefined' && module.exports)
		module.exports = api;
	else
		root.UfcCodec = api;
})(typeof self !== 'undefined' ? self : this);
