// Generates the 9-slice UI textures used by the CommonUI styles in Source/EpicMegaJam2026/UI/DCCommonUI.cpp.
// Usage: node SourceArt/UI/gen_ui_textures.js   (writes the PNGs next to this file; then reimport them in the editor)
// Colors mirror DCPalette. Shapes are drawn on a small "art pixel" grid, then scaled up SCALE times for a chunky pixel look.
// If a size or margin changes here, update the matching SliceBrush call in DCCommonUI.cpp.
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

const SCALE = 2;
const hex = (h, a = 255) => [parseInt(h.slice(0, 2), 16), parseInt(h.slice(2, 4), 16), parseInt(h.slice(4, 6), 16), a];
const mix = (c1, c2, t) => c1.map((v, i) => Math.round(v + (c2[i] - v) * t));
const shade = (c, f) => [Math.round(c[0] * f), Math.round(c[1] * f), Math.round(c[2] * f), c[3]];
const grey = (v) => [Math.round(v * 255), Math.round(v * 255), Math.round(v * 255), 255];

const GREEN = hex('477C55'), BLUE = hex('416DAD'), LIME = hex('A4B65B'), GREY = hex('C5C7BB');
const NAVY = hex('152036', 240), INK = hex('0B1222');
const CLEAR = [0, 0, 0, 0];

// Inset depth of a pixel in a W x H rectangle whose corners are cut by C at 45 degrees (negative = outside)
function chamferDepth(x, y, W, H, C) {
	const dx = Math.min(x, W - 1 - x), dy = Math.min(y, H - 1 - y);
	return Math.min(dx, dy, dx + dy - C);
}

// Panel frame: dark outline, two-tone green line, dark inner line, navy fill. Baked colors (two hues).
function panel() {
	const W = 24, H = 24, C = 4;
	return { W, H, px: (x, y) => {
		const d = chamferDepth(x, y, W, H, C);
		if (d < 0) return CLEAR;
		if (d === 0) return INK;
		if (d === 1) return mix(GREEN, LIME, 0.45);
		if (d <= 3) return GREEN;
		if (d === 4) return INK;
		return NAVY;
	} };
}

// Plate for tabs, buttons and insets: greyscale so the style's tint gives a light border over a darker fill of the same hue
function plate() {
	const W = 18, H = 18, C = 3;
	return { W, H, px: (x, y) => {
		const d = chamferDepth(x, y, W, H, C);
		if (d < 0) return CLEAR;
		if (d === 0) return grey(0.1);
		if (d <= 2) return grey(1.0);
		if (d === 3) return grey(0.22);
		return grey(0.68);
	} };
}

// Title plate: parallelogram leaning right, with a green stripe at its left end. Baked colors (two hues).
function header() {
	const W = 44, H = 26, R = 0.5;
	const left = (y) => Math.round(R * (H - 1 - y));
	const right = (y) => W - 1 - Math.round(R * y);
	const region = (x, y) => {
		if (y < 0 || y >= H) return 0;
		const l = left(y);
		if (x >= l && x <= l + 5) return 1; // stripe
		if (x >= l + 8 && x <= right(y)) return 2; // plate
		return 0;
	};
	return { W, H, px: (x, y) => {
		const r = region(x, y);
		if (!r) return CLEAR;
		const edge = [[1, 0], [-1, 0], [0, 1], [0, -1]].some(([ox, oy]) => region(x + ox, y + oy) !== r);
		if (edge) return INK;
		if (r === 1) return y <= 2 ? mix(GREEN, LIME, 0.7) : mix(GREEN, LIME, 0.35);
		if (y <= 3) return mix(BLUE, GREY, 0.3);
		if (y >= H - 4) return shade(BLUE, 0.75);
		return BLUE;
	} };
}

// ---------------------------------------------------------------- PNG writer (RGBA8)

const CRC = new Uint32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
const crc32 = (buf) => { let c = 0xFFFFFFFF; for (const b of buf) c = CRC[(c ^ b) & 0xFF] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0; };
function chunk(type, data) {
	const out = Buffer.alloc(12 + data.length);
	out.writeUInt32BE(data.length, 0);
	out.write(type, 4, 'ascii');
	data.copy(out, 8);
	out.writeUInt32BE(crc32(out.subarray(4, 8 + data.length)), 8 + data.length);
	return out;
}
function writePng(file, shape) {
	const w = shape.W * SCALE, h = shape.H * SCALE;
	const raw = Buffer.alloc((w * 4 + 1) * h);
	for (let y = 0; y < h; y++) {
		raw[y * (w * 4 + 1)] = 0; // filter: none
		for (let x = 0; x < w; x++) {
			const c = shape.px(Math.floor(x / SCALE), Math.floor(y / SCALE));
			raw.set(c, y * (w * 4 + 1) + 1 + x * 4);
		}
	}
	const ihdr = Buffer.alloc(13);
	ihdr.writeUInt32BE(w, 0);
	ihdr.writeUInt32BE(h, 4);
	ihdr.set([8, 6, 0, 0, 0], 8); // 8-bit RGBA
	fs.writeFileSync(file, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]));
	console.log(`${path.basename(file)} ${w}x${h}`);
}

for (const [name, make] of [['T_UI_Panel', panel], ['T_UI_Plate', plate], ['T_UI_Header', header]]) {
	writePng(path.join(__dirname, `${name}.png`), make());
}
