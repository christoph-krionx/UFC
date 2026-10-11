/*
	UFC editor - edits the configuration of the ufc firmware over web serial.
	Protocol description: firmware/ufc/Protocol.h
*/
(function () {
	'use strict';

	const codec = UfcCodec;

	const PROTOCOL_VERSION = 1;
	const BAUDRATE = 115200;
	const BOOT_WAIT_MS = 3000;

	const FN_NONE = 0, FN_MIDI = 1, FN_PRESET_PREV = 2, FN_PRESET_NEXT = 3;
	const MODE_TOGGLE = 0, MODE_MOMENTARY = 1, MODE_GROUP = 2;
	const MSG_NONE = 0, MSG_PC = 1, MSG_CC = 2;

	const FUNCTIONS = [
		{ value: FN_NONE, label: 'Keine' },
		{ value: FN_MIDI, label: 'MIDI senden' },
		{ value: FN_PRESET_PREV, label: 'Preset −', internal: true },
		{ value: FN_PRESET_NEXT, label: 'Preset +', internal: true }
	];

	const MODES = [
		{ value: MODE_TOGGLE, label: 'Toggle (an / aus)' },
		{ value: MODE_MOMENTARY, label: 'Momentary (solange gedrückt)' },
		{ value: MODE_GROUP, label: 'Gruppe (nur einer aktiv)' }
	];

	const MESSAGE_TYPES = [
		{ value: MSG_NONE, label: 'Keine' },
		{ value: MSG_PC, label: 'Program Change' },
		{ value: MSG_CC, label: 'Control Change' }
	];

	const PORTS = [
		{ value: 3, label: 'MIDI 1 + 2' },
		{ value: 1, label: 'MIDI 1' },
		{ value: 2, label: 'MIDI 2' }
	];

	// same order as enum Color in Config.h
	const COLORS = [
		{ label: 'Aus', css: '#17181a' },
		{ label: 'Rot', css: '#ff2a2a' },
		{ label: 'Orange', css: '#ff7a1a' },
		{ label: 'Gelb', css: '#ffd21a' },
		{ label: 'Grün', css: '#27d943' },
		{ label: 'Cyan', css: '#1fd6e0' },
		{ label: 'Blau', css: '#3a6bff' },
		{ label: 'Violett', css: '#a14bff' },
		{ label: 'Weiß', css: '#f4f4f4' }
	];

	const state = {
		layout: Object.assign({}, codec.DEFAULT_LAYOUT),
		global: null,
		presets: null,
		dirtyGlobal: false,
		dirtyPresets: [],
		preset: 0,		// preset shown in the editor
		selected: null,		// selected switch index or null for the preset settings
		copiedSwitch: null,
		copiedPreset: null,
		connected: false,
		busy: false
	};

	const $ = (id) => document.getElementById(id);

	//
	// serial connection
	//

	const device = {
		port: null,
		reader: null,
		pending: null,
		queue: Promise.resolve(),

		async open() {
			this.port = await navigator.serial.requestPort();
			this.linesSeen = 0;
			this.presetSeen = false;

			try {
				await this.port.open({ baudRate: BAUDRATE });
			} catch (error) {
				this.port = null;
				throw new Error('Der Anschluss lässt sich nicht öffnen. Ist er noch in einem anderen Programm geöffnet (serieller Monitor, Arduino IDE, zweiter Browser-Tab)?');
			}

			this.reading = this.readLoop();
		},

		// resets the controller like the arduino ide does
		async reset() {
			await this.port.setSignals({ dataTerminalReady: true });
			await sleep(100);
			await this.port.setSignals({ dataTerminalReady: false });
		},

		// the port can only be closed after the read loop has released it
		async close() {
			const port = this.port;

			try {
				if (this.reader)
					await this.reader.cancel();
				await this.reading;
			} catch (error) {
				// the port is gone anyway
			}

			try {
				if (port)
					await port.close();
			} catch (error) {
				// already closed or unplugged
			}

			this.port = null;
		},

		async readLoop() {
			const decoder = new TextDecoder();
			let buffer = '';

			try {
				this.reader = this.port.readable.getReader();

				for (;;) {
					const { value, done } = await this.reader.read();
					if (done)
						break;

					buffer += decoder.decode(value, { stream: true });

					let end;
					while ((end = buffer.indexOf('\n')) >= 0) {
						const line = buffer.slice(0, end).replace(/\r$/, '');
						buffer = buffer.slice(end + 1);
						if (line.length > 0)
							this.onLine(line);
					}
				}
			} catch (error) {
				// handled below as a lost connection
			} finally {
				if (this.reader)
					this.reader.releaseLock();
				this.reader = null;
				onDisconnected();
			}
		},

		onLine(line) {
			this.linesSeen++;

			if (line.startsWith('!cur ')) {
				this.presetSeen = true;
				onDevicePreset(parseInt(line.slice(5), 10));
			} else if (line.startsWith('!') && this.pending) {
				const pending = this.pending;
				this.pending = null;
				clearTimeout(pending.timer);

				if (line.startsWith('!err'))
					pending.reject(new Error('Gerät meldet: ' + line.slice(5)));
				else
					pending.resolve(line);
			} else {
				log(line);
			}
		},

		// commands are executed one after the other
		request(command, timeout = 5000) {
			const run = () => new Promise((resolve, reject) => {
				const timer = setTimeout(() => {
					this.pending = null;
					reject(new Error('Keine Antwort auf "' + command.split(':')[0] + '"'));
				}, timeout);

				this.pending = { resolve, reject, timer };
				this.write(command + '\n').catch((error) => {
					clearTimeout(timer);
					this.pending = null;
					reject(error);
				});
			});

			const result = this.queue.then(run, run);
			this.queue = result.catch(() => {});
			return result;
		},

		// the receive buffer of the controller holds 64 bytes, so long lines are sent in small pieces
		async write(text) {
			const writer = this.port.writable.getWriter();
			const bytes = new TextEncoder().encode(text);

			try {
				for (let i = 0; i < bytes.length; i += 32) {
					await writer.write(bytes.subarray(i, i + 32));
					if (bytes.length > 32)
						await sleep(4);
				}
			} finally {
				writer.releaseLock();
			}
		}
	};

	function sleep(ms) {
		return new Promise((resolve) => setTimeout(resolve, ms));
	}

	function parseInfo(line) {
		const info = {};
		for (const part of line.split(' ').slice(1)) {
			const [key, value] = part.split('=');
			info[key] = value;
		}
		return info;
	}

	async function connect() {
		await device.open();
		setStatus('Warte auf das Gerät …');

		// Opening the port may reset the controller. Nothing must be sent while its bootloader is running,
		// received data keeps the bootloader active and the firmware never starts.
		let info = await waitForInfo();

		if (!info && device.port) {
			await device.reset();
			info = await waitForInfo();
		}

		if (!info)
			throw new Error('Das Gerät antwortet nicht. Läuft die ufc-Firmware?');

		if (parseInt(info.proto, 10) !== PROTOCOL_VERSION)
			throw new Error('Protokollversion ' + info.proto + ' wird von diesem Editor nicht unterstützt');

		const layout = {};
		for (const key of Object.keys(codec.DEFAULT_LAYOUT))
			layout[key] = parseInt(info[key], 10);

		if (codec.globalSize(layout) !== parseInt(info.gsize, 10) || codec.presetSize(layout) !== parseInt(info.psize, 10))
			throw new Error('Das Datenformat der Firmware ' + info.ver + ' passt nicht zu diesem Editor');

		state.layout = layout;
		state.connected = true;
		state.firmware = info.ver;
		state.preset = parseInt(info.cur, 10) || 0;

		await readAll();
	}

	// waits until the firmware has started, then asks for the device information
	async function waitForInfo() {
		const linesBefore = device.linesSeen;

		device.presetSeen = false;
		await sleep(BOOT_WAIT_MS);

		// log output without the preset message: the firmware is still starting (first start writes the whole eeprom)
		if (device.linesSeen > linesBefore && !device.presetSeen) {
			for (let waited = 0; waited < 40000 && !device.presetSeen && device.port; waited += 200)
				await sleep(200);
		}

		for (let attempt = 0; attempt < 2 && device.port; attempt++) {
			try {
				return parseInfo(await device.request('info', 1500));
			} catch (error) {
				// no answer, try again
			}
		}

		return null;
	}

	async function readAll() {
		const layout = state.layout;

		setStatus('Lese Konfiguration …');
		const globalLine = await device.request('get g');
		const global = codec.decodeGlobal(layout, codec.hexToBlock(globalLine.slice(globalLine.indexOf(':') + 1), codec.globalSize(layout)));

		const presets = [];
		for (let i = 0; i < layout.presets; i++) {
			const line = await device.request('get p ' + i);
			presets.push(codec.decodePreset(layout, codec.hexToBlock(line.slice(line.indexOf(':') + 1), codec.presetSize(layout))));
		}

		state.global = global;
		state.presets = presets;
		state.dirtyGlobal = false;
		state.dirtyPresets = presets.map(() => false);
		state.selected = null;
	}

	async function writeDirty() {
		const layout = state.layout;

		if (state.dirtyGlobal) {
			setStatus('Sende globale Einstellungen …');
			await device.request('set g:' + codec.blockToHex(codec.encodeGlobal(layout, state.global)), 10000);
			state.dirtyGlobal = false;
		}

		for (let i = 0; i < state.presets.length; i++) {
			if (!state.dirtyPresets[i])
				continue;

			setStatus('Sende Preset ' + (i + 1) + ' …');
			await device.request('set p ' + i + ':' + codec.blockToHex(codec.encodePreset(layout, state.presets[i])), 10000);
			state.dirtyPresets[i] = false;
		}
	}

	function onDisconnected() {
		if (device.pending) {
			clearTimeout(device.pending.timer);
			device.pending.reject(new Error('Verbindung verloren'));
			device.pending = null;
		}

		device.port = null;
		state.connected = false;
		render();
	}

	// the controller reports every preset change, the editor follows it
	function onDevicePreset(index) {
		if (!state.presets || index === state.preset || !(index >= 0 && index < state.presets.length))
			return;

		state.preset = index;
		state.selected = null;
		render();
	}

	//
	// model helpers
	//

	function isDirty() {
		return state.dirtyGlobal || state.dirtyPresets.some((dirty) => dirty);
	}

	function isGlobal(index) {
		return index >= state.layout.foot || ((state.global.mask >>> index) & 1) === 1;
	}

	function configOf(index) {
		return isGlobal(index) ? state.global.switches[index] : state.presets[state.preset].switches[index];
	}

	function markDirty(index) {
		if (index === null || !isGlobal(index))
			state.dirtyPresets[state.preset] = true;
		else
			state.dirtyGlobal = true;
	}

	// the definition is copied to the other side so that the switch keeps its function
	function setGlobal(index, global) {
		const preset = state.presets[state.preset];

		if (global) {
			state.global.switches[index] = JSON.parse(JSON.stringify(preset.switches[index]));
			state.global.switches[index].initialOn = false;
			state.global.mask = (state.global.mask | (1 << index)) >>> 0;
		} else {
			const copy = JSON.parse(JSON.stringify(state.global.switches[index]));
			if (copy.fn !== FN_NONE && copy.fn !== FN_MIDI)
				copy.fn = FN_NONE;
			preset.switches[index] = copy;
			state.global.mask = (state.global.mask & ~(1 << index)) >>> 0;
			state.dirtyPresets[state.preset] = true;
		}

		state.dirtyGlobal = true;
	}

	function clone(value) {
		return JSON.parse(JSON.stringify(value));
	}

	function emptyMessage() {
		return { type: MSG_NONE, port: 0, channel: 0, number: 0, valueOn: 0, valueOff: 0 };
	}

	function emptySwitch() {
		const config = { name: '', fn: FN_NONE, mode: MODE_TOGGLE, initialOn: false, color: 0, messages: [] };
		for (let i = 0; i < state.layout.msgs; i++)
			config.messages.push(emptyMessage());
		return config;
	}

	// removes all assignments of the preset, the name stays
	function clearPreset(preset) {
		preset.start = preset.start.map(emptyMessage);
		preset.switches = preset.switches.map(emptySwitch);
	}

	// the target keeps its place (global or preset), internal functions only work on global switches
	function pasteSwitch(index) {
		const copy = clone(state.copiedSwitch);

		if (isGlobal(index)) {
			copy.initialOn = false;
			state.global.switches[index] = copy;
		} else {
			if (copy.fn !== FN_NONE && copy.fn !== FN_MIDI)
				copy.fn = FN_NONE;
			state.presets[state.preset].switches[index] = copy;
		}

		markDirty(index);
	}

	function switchLabel(index) {
		return index < state.layout.foot ? 'Taster ' + (index + 1) : 'Extern ' + (index - state.layout.foot + 1);
	}

	function messageText(message, on) {
		if (message.type === MSG_PC)
			return 'PC ' + message.number;
		if (message.type === MSG_CC)
			return 'CC ' + message.number + (on === undefined ? '' : '=' + (on ? message.valueOn : message.valueOff));
		return '';
	}

	function summaryOf(config) {
		if (config.fn === FN_PRESET_PREV)
			return 'Preset −';
		if (config.fn === FN_PRESET_NEXT)
			return 'Preset +';
		if (config.fn !== FN_MIDI)
			return 'ohne Funktion';

		const texts = config.messages.filter((m) => m.type !== MSG_NONE).map((m) => messageText(m));
		return texts.length ? texts.join(', ') : 'keine Nachricht';
	}

	// mixes the color with the unlit led for the dimmed function led
	function dimmed(css) {
		const value = parseInt(css.slice(1), 16);
		const mix = (channel, off) => Math.round(off + (channel - off) * 0.35);
		const r = mix((value >> 16) & 0xFF, 0x17), g = mix((value >> 8) & 0xFF, 0x18), b = mix(value & 0xFF, 0x1a);
		return 'rgb(' + r + ',' + g + ',' + b + ')';
	}

	//
	// rendering
	//

	function el(tag, attributes, children) {
		const node = document.createElement(tag);

		for (const [key, value] of Object.entries(attributes || {})) {
			if (key === 'class')
				node.className = value;
			else if (key === 'text')
				node.textContent = value;
			else if (key.startsWith('on'))
				node.addEventListener(key.slice(2), value);
			else if (value !== false && value !== null && value !== undefined)
				node.setAttribute(key, value === true ? '' : value);
		}

		for (const child of children || [])
			node.append(child);

		return node;
	}

	function setStatus(text, dirty) {
		$('status').textContent = text;
		$('status').className = dirty ? 'dirty' : '';
	}

	function log(line) {
		const text = $('logText');
		text.textContent += line.replace(/^# ?/, '') + '\n';
		text.scrollTop = text.scrollHeight;
	}

	function renderHeader() {
		const loaded = state.presets !== null;

		$('btnConnect').textContent = state.connected ? 'Trennen' : 'Verbinden';
		$('btnConnect').className = state.connected ? '' : 'primary';
		$('btnConnect').disabled = state.busy || !('serial' in navigator);
		$('btnRead').disabled = state.busy || !state.connected;
		$('btnWrite').disabled = state.busy || !state.connected || !loaded || !isDirty();
		$('btnWrite').className = (state.connected && loaded && isDirty()) ? 'primary' : '';
		$('btnOpen').disabled = state.busy;
		$('btnSave').disabled = state.busy || !loaded;
		$('btnReset').disabled = state.busy || !state.connected;

		if (state.busy)
			return;

		if (loaded && isDirty())
			setStatus(state.connected ? 'Ungesendete Änderungen' : 'Ungesendete Änderungen, nicht verbunden', true);
		else if (state.connected)
			setStatus('Verbunden, Firmware ' + state.firmware);
		else
			setStatus('Nicht verbunden');
	}

	function renderPresets() {
		const container = $('presets');
		container.replaceChildren();

		if (!state.presets)
			return;

		state.presets.forEach((preset, index) => {
			container.append(el('button', {
				class: index === state.preset ? 'active' : '',
				onclick: () => selectPreset(index)
			}, [
				el('span', { class: 'num', text: String(index + 1) }),
				el('span', { text: preset.name || '(ohne Name)' }),
				state.dirtyPresets[index] ? el('span', { class: 'flag', text: '●', title: 'geändert' }) : ''
			]));
		});
	}

	function renderTile(index) {
		const config = configOf(index);
		const used = config.fn !== FN_NONE;
		const color = COLORS[config.color] ? COLORS[config.color].css : COLORS[0].css;
		const children = [
			el('div', { class: 'num', text: switchLabel(index) }),
			el('div', { class: 'name', text: config.name || ' ' }),
			el('div', { class: 'summary', text: summaryOf(config) })
		];

		if (isGlobal(index))
			children.push(el('span', { class: 'global', text: 'GLOBAL', title: 'In allen Presets gleich' }));

		if (index < state.layout.foot) {
			const left = el('span', { class: 'led', title: 'Funktion' });
			const right = el('span', { class: 'led', title: 'Zustand beim Laden des Presets' });

			if (used) {
				left.style.background = dimmed(color);
				if (config.fn === FN_MIDI && config.initialOn && !isGlobal(index))
					right.style.background = color;
			}

			children.push(el('div', { class: 'leds' }, [left, right]));
		}

		return el('button', {
			class: 'tile' + (state.selected === index ? ' selected' : '') + (used ? '' : ' unused'),
			onclick: () => { state.selected = index; render(); }
		}, children);
	}

	function renderLcd() {
		const preset = state.presets[state.preset];
		let active = '';

		for (let i = 0; i < state.layout.foot + state.layout.ext; i++) {
			const config = configOf(i);
			if (config.fn === FN_MIDI && config.mode === MODE_GROUP && config.initialOn && !isGlobal(i)) {
				active = '> ' + config.name;
				break;
			}
		}

		// the preset number is shown inverted like on the lcd
		return el('div', { id: 'lcd', title: 'Anzeige nach dem Laden des Presets' }, [
			el('span', { class: 'inverted', text: String((state.preset + 1) % 10) }),
			(' ' + preset.name).padEnd(19).slice(0, 19) + '\n' + active.padEnd(20).slice(0, 20)
		]);
	}

	// the board is drawn like the hardware: three switches in the top row, then three rows of six
	function renderBoard() {
		const board = $('board');
		board.replaceChildren();

		if (!state.presets)
			return;

		const top = el('div', { class: 'row' }, [renderLcd()]);
		for (let i = 18; i < state.layout.foot; i++)
			top.append(renderTile(i));
		board.append(top);

		for (const first of [12, 6, 0]) {
			const row = el('div', { class: 'row' });
			for (let i = first; i < first + 6; i++)
				row.append(renderTile(i));
			board.append(row);
		}

		const ext = el('div', { class: 'row ext' });
		for (let i = state.layout.foot; i < state.layout.foot + state.layout.ext; i++)
			ext.append(renderTile(i));
		board.append(el('div', {}, [el('p', { class: 'rowlabel', text: 'Externe Taster (immer global)' }), ext]));
	}

	function field(label, control) {
		return el('label', { class: 'field' }, [el('span', { text: label }), control]);
	}

	function select(options, value, onChange) {
		const node = el('select', { onchange: () => onChange(parseInt(node.value, 10)) });
		for (const option of options)
			node.append(el('option', { value: option.value, text: option.label, selected: option.value === value }));
		return node;
	}

	function numberInput(value, min, max, onChange) {
		const node = el('input', { type: 'number', min: min, max: max, value: value });
		node.addEventListener('input', () => {
			const number = parseInt(node.value, 10);
			if (!isNaN(number))
				onChange(Math.min(max, Math.max(min, number)));
		});
		node.addEventListener('blur', () => {
			const number = parseInt(node.value, 10);
			node.value = isNaN(number) ? min : Math.min(max, Math.max(min, number));
		});
		return node;
	}

	function nameInput(value, length, onChange) {
		const node = el('input', { type: 'text', class: 'name', maxlength: length, value: value, spellcheck: 'false' });
		node.addEventListener('input', () => {
			// the lcd only shows printable ascii characters
			const clean = node.value.replace(/[^\x20-\x7E]/g, '');
			if (clean !== node.value)
				node.value = clean;
			onChange(clean);
		});
		return node;
	}

	// changed is called after every edit, structural changes redraw the panel
	function renderMessage(message, title, changed, redraw) {
		const box = el('div', { class: 'message' });

		box.append(field(title, select(MESSAGE_TYPES, message.type, (value) => {
			// a new message goes to both outputs and switches a controller fully on
			if (message.type === MSG_NONE && message.port === 0)
				message.port = 3;
			if (value === MSG_CC && message.valueOn === 0 && message.valueOff === 0)
				message.valueOn = 127;
			message.type = value;
			changed();
			redraw();
		})));

		if (message.type === MSG_NONE)
			return box;

		const grid = el('div', { class: 'grid' });
		const cell = (label, control) => el('div', {}, [el('label', { text: label }), control]);

		grid.append(cell('Ausgang', select(PORTS, message.port, (value) => { message.port = value; changed(); })));
		grid.append(cell('Kanal', numberInput(message.channel + 1, 1, 16, (value) => { message.channel = value - 1; changed(); })));
		grid.append(cell(message.type === MSG_PC ? 'Programm' : 'Controller', numberInput(message.number, 0, 127, (value) => { message.number = value; changed(); })));

		if (message.type === MSG_CC) {
			grid.append(el('div'));
			grid.append(cell('Wert an', numberInput(message.valueOn, 0, 127, (value) => { message.valueOn = value; changed(); })));
			grid.append(cell('Wert aus', numberInput(message.valueOff, 0, 127, (value) => { message.valueOff = value; changed(); })));
		}

		box.append(grid);
		return box;
	}

	function renderPresetPanel(panel) {
		const preset = state.presets[state.preset];
		const changed = () => { state.dirtyPresets[state.preset] = true; renderLive(); };

		panel.append(el('h2', { text: 'Preset ' + (state.preset + 1) }));
		panel.append(field('Name', nameInput(preset.name, state.layout.plen, (value) => { preset.name = value; changed(); })));
		panel.append(el('h3', { text: 'Beim Laden senden' }));

		preset.start.forEach((message, index) => {
			panel.append(renderMessage(message, 'Nachricht ' + (index + 1), changed, render));
		});

		panel.append(el('p', { class: 'hint', text: 'Einen Taster anklicken, um ihn zu bearbeiten.' }));

		panel.append(el('h3', { text: 'Ganzes Preset' }));
		panel.append(el('div', { class: 'actions' }, [
			el('button', { text: 'Kopieren', onclick: () => { state.copiedPreset = clone(preset); render(); } }),
			el('button', {
				text: 'Einfügen',
				disabled: state.copiedPreset === null,
				title: state.copiedPreset ? 'Fügt „' + state.copiedPreset.name + '" hier ein' : 'Erst ein Preset kopieren',
				onclick: () => {
					if (!confirm('Preset ' + (state.preset + 1) + ' mit der Kopie von „' + state.copiedPreset.name + '" überschreiben?'))
						return;
					state.presets[state.preset] = clone(state.copiedPreset);
					state.dirtyPresets[state.preset] = true;
					render();
				}
			}),
			el('button', {
				text: 'Leeren',
				onclick: () => {
					if (!confirm('Alle Taster und Startnachrichten von Preset ' + (state.preset + 1) + ' löschen?'))
						return;
					clearPreset(preset);
					state.dirtyPresets[state.preset] = true;
					render();
				}
			})
		]));
		panel.append(el('p', { class: 'hint', text: 'Globale Taster gehören nicht zum Preset und bleiben unverändert.' }));
	}

	function renderSwitchPanel(panel, index) {
		const config = configOf(index);
		const global = isGlobal(index);
		const foot = index < state.layout.foot;
		const changed = () => { markDirty(index); renderLive(); };

		panel.append(el('h2', { text: switchLabel(index) + (global ? ' (global)' : ' in Preset ' + (state.preset + 1)) }));

		if (foot) {
			const checkbox = el('input', { type: 'checkbox', checked: global });
			checkbox.addEventListener('change', () => { setGlobal(index, checkbox.checked); render(); });
			panel.append(el('label', { class: 'check' }, [checkbox, el('span', { text: 'Global: in allen Presets gleich' })]));
		}

		panel.append(field('Name', nameInput(config.name, state.layout.slen, (value) => { config.name = value; changed(); })));

		// internal functions are reserved for global switches
		const functions = FUNCTIONS.filter((fn) => global || !fn.internal);
		panel.append(field('Funktion', select(functions, config.fn, (value) => { config.fn = value; changed(); render(); })));

		if (config.fn === FN_NONE) {
			panel.append(backLink());
			return;
		}

		const swatches = el('div', { class: 'swatches' });
		COLORS.forEach((color, value) => {
			if (value === 0)
				return;
			const swatch = el('button', {
				class: 'swatch' + (config.color === value ? ' active' : ''),
				title: color.label,
				'aria-label': color.label,
				onclick: () => { config.color = value; changed(); render(); }
			});
			swatch.style.background = color.css;
			swatches.append(swatch);
		});
		panel.append(field('LED-Farbe', swatches));

		if (config.fn === FN_MIDI) {
			panel.append(field('Modus', select(MODES, config.mode, (value) => { config.mode = value; changed(); render(); })));

			if (!global && config.mode !== MODE_MOMENTARY) {
				const checkbox = el('input', { type: 'checkbox', checked: config.initialOn });
				checkbox.addEventListener('change', () => { config.initialOn = checkbox.checked; changed(); });
				panel.append(el('label', { class: 'check' }, [checkbox, el('span', { text: 'Beim Laden des Presets an' })]));
			}

			panel.append(el('h3', { text: 'Nachrichten' }));
			config.messages.forEach((message, number) => {
				panel.append(renderMessage(message, 'Nachricht ' + (number + 1), changed, render));
			});

			if (config.mode === MODE_GROUP)
				panel.append(el('p', { class: 'hint', text: 'Gruppe: Beim Drücken werden die An-Werte gesendet, die anderen Gruppentaster gehen aus.' }));
		}

		panel.append(backLink());
	}

	// copy and paste for the selected switch and the way back to the preset settings
	function backLink() {
		const index = state.selected;

		return el('div', { class: 'actions' }, [
			el('button', { text: 'Kopieren', onclick: () => { state.copiedSwitch = clone(configOf(index)); render(); } }),
			el('button', {
				text: 'Einfügen',
				disabled: state.copiedSwitch === null,
				title: state.copiedSwitch ? 'Fügt „' + state.copiedSwitch.name + '" hier ein' : 'Erst einen Taster kopieren',
				onclick: () => { pasteSwitch(index); render(); }
			}),
			el('button', { text: 'Preset-Einstellungen', onclick: () => { state.selected = null; render(); } })
		]);
	}

	function renderPanel() {
		const panel = $('panel');
		panel.replaceChildren();

		if (!state.presets) {
			panel.append(el('h2', { text: 'Keine Konfiguration geladen' }));
			panel.append(el('p', { class: 'hint', text: 'Controller per USB anschließen und „Verbinden" wählen, oder eine gesicherte Datei öffnen.' }));
			return;
		}

		if (state.selected === null)
			renderPresetPanel(panel);
		else
			renderSwitchPanel(panel, state.selected);
	}

	// redraws everything except the panel so that the focused input keeps its cursor
	function renderLive() {
		renderHeader();
		renderPresets();
		renderBoard();
	}

	function render() {
		renderLive();
		renderPanel();
	}

	//
	// actions
	//

	function showNotice(text) {
		$('notice').textContent = text || '';
	}

	async function run(action) {
		if (state.busy)
			return;

		state.busy = true;
		showNotice('');
		renderHeader();

		try {
			await action();
		} catch (error) {
			// a cancelled port dialog is not an error
			if (error.name !== 'NotFoundError')
				showNotice(error.message);
		}

		state.busy = false;
		render();
	}

	function selectPreset(index) {
		state.preset = index;
		state.selected = null;
		render();

		// the controller follows the editor
		if (state.connected)
			device.request('load ' + index).catch((error) => showNotice(error.message));
	}

	function saveFile() {
		const data = { format: 'ufc-config', version: 1, layout: state.layout, global: state.global, presets: state.presets };
		const url = URL.createObjectURL(new Blob([JSON.stringify(data, null, '\t')], { type: 'application/json' }));
		const link = el('a', { href: url, download: 'ufc-config.json' });

		link.click();
		URL.revokeObjectURL(url);
	}

	async function openFile(file) {
		const data = JSON.parse(await file.text());

		if (data.format !== 'ufc-config' || !data.global || !Array.isArray(data.presets))
			throw new Error('Das ist keine UFC-Konfigurationsdatei');

		const layout = state.connected ? state.layout : data.layout;
		if (JSON.stringify(data.layout) !== JSON.stringify(layout))
			throw new Error('Die Datei passt nicht zum Datenformat des angeschlossenen Geräts');

		// encoding and decoding again brings every value into its valid range
		state.layout = layout;
		state.global = codec.decodeGlobal(layout, codec.encodeGlobal(layout, data.global));
		state.presets = data.presets.map((preset) => codec.decodePreset(layout, codec.encodePreset(layout, preset)));
		state.dirtyGlobal = true;
		state.dirtyPresets = state.presets.map(() => true);
		state.preset = Math.min(state.preset, state.presets.length - 1);
		state.selected = null;
	}

	$('btnConnect').addEventListener('click', () => run(async () => {
		if (state.connected || device.port) {
			await device.close();
			return;
		}

		try {
			await connect();
		} catch (error) {
			await device.close();
			throw error;
		}
	}));

	$('btnRead').addEventListener('click', () => run(async () => {
		if (isDirty() && !confirm('Ungesendete Änderungen im Editor verwerfen?'))
			return;
		await readAll();
	}));

	$('btnWrite').addEventListener('click', () => run(writeDirty));

	$('btnReset').addEventListener('click', () => run(async () => {
		if (!confirm('Alle Presets und Einstellungen im Gerät auf Werkseinstellungen zurücksetzen?'))
			return;

		setStatus('Setze zurück, das dauert einige Sekunden …');
		await device.request('reset', 40000);
		state.preset = 0;
		await readAll();
	}));

	$('btnSave').addEventListener('click', saveFile);
	$('btnOpen').addEventListener('click', () => $('fileInput').click());

	$('fileInput').addEventListener('change', () => run(async () => {
		const file = $('fileInput').files[0];
		$('fileInput').value = '';
		if (file)
			await openFile(file);
	}));

	window.addEventListener('beforeunload', (event) => {
		if (state.presets && isDirty())
			event.preventDefault();
	});

	if (!('serial' in navigator))
		showNotice('Dieser Browser unterstützt Web Serial nicht. Bitte Chrome oder Edge am PC verwenden. Dateien lassen sich trotzdem öffnen und bearbeiten.');

	// for tests in the browser console
	window.ufcEditor = { state, render, codec };

	render();
})();
