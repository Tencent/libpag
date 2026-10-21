// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
// Rebuild the small, original test fonts; no external font files or network are used.
// Run after npm ci in tools/html-snapshot: node test/scripts/generate-color-font-fixtures.cjs
const fs = require('node:fs');
const path = require('node:path');
const ot = require('../../tools/html-snapshot/node_modules/opentype.js');
const out = path.resolve(__dirname, '../../resources/font/test');
fs.mkdirSync(out, { recursive: true });
const u16 = n => { const b = Buffer.alloc(2); b.writeUInt16BE(n & 65535); return b; };
const u24 = n => Buffer.from([n >> 16 & 255, n >> 8 & 255, n & 255]);
const u32 = n => { const b = Buffer.alloc(4); b.writeUInt32BE(n >>> 0); return b; };
const cat = (...b) => Buffer.concat(b.flat().map(x => typeof x === 'number' ? Buffer.from([x]) : x));
const f2 = n => u16(Math.round(n * 16384));
const fixed = n => u32(Math.round(n * 65536));
const box = new ot.Path();
box.moveTo(0, 0); box.lineTo(1000, 0); box.lineTo(1000, 1000); box.lineTo(0, 1000); box.close();
const chars = [' ', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', '0', '#', '*'];
const glyphs = [new ot.Glyph({ name: '.notdef', advanceWidth: 1000, path: box })];
for (const c of chars) glyphs.push(new ot.Glyph({ name: `g${c.codePointAt(0)}`, unicode: c.codePointAt(0),
  advanceWidth: c === ' ' ? 500 : 1000, path: c === ' ' ? new ot.Path() : box }));
for (let mode = 0; mode < 28; ++mode) glyphs.push(new ot.Glyph({ name: `blend${mode}`,
  unicode: 0xe000 + mode, advanceWidth: 1000, path: box }));
const ids = new Map(glyphs.map((g, i) => [g.unicode, i]));
const gid = c => ids.get(c.codePointAt(0));
const font = new ot.Font({ familyName: 'Color Paint Fixture', styleName: 'Regular',
  unitsPerEm: 1000, ascender: 1000, descender: 0, glyphs });
const base = Buffer.from(font.toArrayBuffer());
const solid = (index, alpha = 1) => cat(2, u16(index), f2(alpha));
const paintGlyph = (p, id = gid('B')) => cat(10, u24(6), u16(id), p);
const line = (extend = 0, a = 0, b = 1) => cat(extend, u16(2), f2(0), u16(a), f2(1), f2(1), u16(b), f2(1));
const linear = extend => cat(4, u24(16), [0, 0, 1000, 0, 0, 1000].map(u16), line(extend));
const radial = cat(6, u24(16), [500, 500, 0, 500, 500, 500].map(u16), line());
const sweep = cat(8, u24(12), u16(500), u16(500), f2(-1), f2(0), line());
const translate = (p, x, y) => cat(14, u24(8), u16(x), u16(y), p);
const scale = (p, x, y) => cat(16, u24(8), f2(x), f2(y), p);
const rotate = (p, angle) => cat(24, u24(6), f2(angle / 180), p);
const skew = (p, x, y) => cat(28, u24(8), f2(x / 180), f2(y / 180), p);
const transform = (p, values) => cat(12, u24(31), u24(7), values.map(fixed), p);
const composite = (s, d, mode) => cat(32, u24(8), mode, u24(8 + s.length), s, d);
const paints = new Map([
  [gid('A'), paintGlyph(linear(0))],
  [gid('C'), paintGlyph(solid(0xffff))],
  [gid('D'), paintGlyph(radial)],
  [gid('E'), paintGlyph(sweep)],
  [gid('F'), translate(scale(paintGlyph(solid(0)), .5, .5), 200, 100)],
  [gid('G'), translate(rotate(scale(paintGlyph(solid(1)), .5, .5), 90), 600, 100)],
  [gid('H'), transform(paintGlyph(solid(2)), [.5, 0, .25, .5, 100, 100])],
  [gid('I'), skew(scale(paintGlyph(solid(0)), .5, .5), 30, 0)],
  [gid('J'), cat(11, u16(gid('A')))],
  [gid('K'), paintGlyph(scale(linear(1), .25, 1))],
  [gid('L'), paintGlyph(scale(linear(2), .25, 1))],
  [gid('M'), cat(1, 2, u32(0))],
  [gid('N'), paintGlyph(composite(solid(0, .5), solid(2), 3))],
  [gid('O'), translate(paintGlyph(solid(0)), 1200, 0)],
  ...['0', '#', '*'].map(c => [gid(c), paintGlyph(solid(2))]),
]);
for (let mode = 0; mode < 28; ++mode) paints.set(ids.get(0xe000 + mode),
  paintGlyph(composite(solid(0, .5), solid(2, .5), mode)));
const layers = [paintGlyph(solid(1)), translate(scale(paintGlyph(solid(0)), .5, .5), 250, 250)];
function offsetList(items, stride, prefix) {
  let offset = prefix.length + items.length * stride;
  const records = items.map(([id, p]) => {
    const r = stride === 6 ? cat(u16(id), u32(offset)) : u32(offset);
    offset += p.length; return r;
  });
  return cat(prefix, records, items.map(([, p]) => p));
}
const baseList = offsetList([...paints].sort((a, b) => a[0] - b[0]), 6, u32(paints.size));
const layerList = offsetList(layers.map((p, i) => [i, p]), 4, u32(layers.length));
const clipList = cat(1, u32(3),
  u16(1), u16(gid('O') - 1), u24(26),
  u16(gid('O')), u16(gid('O')), u24(35),
  u16(gid('O') + 1), u16(glyphs.length - 1), u24(26),
  1, [0, 0, 1000, 1000].map(u16), 1, [1200, 0, 2200, 1000].map(u16));
const colr = cat(u16(1), u16(0), u32(0), u32(0), u16(0), u32(34),
  u32(34 + baseList.length), u32(34 + baseList.length + layerList.length), u32(0), u32(0),
  baseList, layerList, clipList);
// Two palettes make palette selection and cache separation observable.
const cpal = cat(u16(0), u16(3), u16(2), u16(6), u32(16), u16(0), u16(3),
  Buffer.from([0, 0, 255, 255, 0, 255, 0, 255, 255, 0, 0, 255,
    255, 255, 0, 255, 255, 0, 255, 255, 0, 255, 255, 255]));
const svgDoc = Buffer.from(`<svg xmlns="http://www.w3.org/2000/svg"><g id="glyph${gid('A')}"><rect x="0" y="-1000" width="1000" height="1000" fill="purple"/></g></svg>`);
const svg = cat(u16(0), u32(10), u32(0), u16(1), u16(gid('A')), u16(gid('A')),
  u32(14), u32(svgDoc.length), svgDoc);
function checksum(b) {
  const padded = Buffer.alloc((b.length + 3) & ~3); b.copy(padded);
  let sum = 0;
  for (let i = 0; i < padded.length; i += 4) sum = (sum + padded.readUInt32BE(i)) >>> 0;
  return sum;
}
function write(name, extra) {
  const tables = new Map();
  for (let i = 0; i < base.readUInt16BE(4); ++i) {
    const pos = 12 + i * 16, offset = base.readUInt32BE(pos + 8), length = base.readUInt32BE(pos + 12);
    tables.set(base.toString('ascii', pos, pos + 4), Buffer.from(base.subarray(offset, offset + length)));
  }
  tables.get('head').writeUInt32BE(0, 8);
  tables.get('head').fill(0, 20, 36); // Stable timestamps for reproducible fixtures.
  for (const [tag, data] of Object.entries(extra)) tables.set(tag, data);
  const entries = [...tables].sort(([a], [b]) => a < b ? -1 : a > b ? 1 : 0);
  const power = 2 ** Math.floor(Math.log2(entries.length));
  let offset = 12 + entries.length * 16, headOffset;
  const directory = [], data = [];
  for (const [tag, bytes] of entries) {
    if (tag === 'head') headOffset = offset;
    directory.push(cat(Buffer.from(tag), u32(checksum(bytes)), u32(offset), u32(bytes.length)));
    const padded = Buffer.alloc((bytes.length + 3) & ~3); bytes.copy(padded); data.push(padded);
    offset += padded.length;
  }
  const result = cat(Buffer.from('OTTO'), u16(entries.length), u16(power * 16),
    u16(Math.log2(power)), u16(entries.length * 16 - power * 16), directory, data);
  result.writeUInt32BE((0xb1b0afba - checksum(result)) >>> 0, headOffset + 8);
  fs.writeFileSync(path.join(out, name), result);
}
write('ColorPaints.otf', { COLR: colr, CPAL: cpal, 'SVG ': svg });
const v0Records = ['A', '0', '#', '*'].map(c => gid(c)).sort((a, b) => a - b);
const v0 = cat(u16(0), u16(v0Records.length), u32(14), u32(14 + v0Records.length * 6), u16(2),
  v0Records.map(id => cat(u16(id), u16(0), u16(2))),
  u16(gid('B')), u16(0), u16(gid('B')), u16(0xffff));
write('ColorLayers.otf', { COLR: v0, CPAL: cpal });
write('UnsupportedSVG.otf', { 'SVG ': svg });
