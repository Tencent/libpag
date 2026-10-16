// Copyright (C) 2026 Tencent. All rights reserved.

// Generated web-font fixtures for the font capture tests.
//
// Fixtures are built at run time instead of being committed as binaries:
// opentype.js writes a one-glyph SFNT whose `name` table carries the requested
// family, and wawoff2's WASM encoder compresses it to WOFF2. That makes the
// tests independent of any font shipped in the repository and lets a test pin
// the exact CSS-name/font-file-name mismatch it needs (e.g. a CSS alias
// `MaShanZheng` serving a file whose family is "Ma Shan Zheng").

'use strict';

// The two codec packages are required directly rather than through
// lib/font-codec: font-download.test.js installs `jest.doMock` stubs for that
// module in one of its describes, and the mock registry survives
// `jest.resetModules()`, so going through the loader would hand this helper a
// stub with no encoder. Fixture bytes are independent of which module instance
// produced them.
const opentype = require('opentype.js');
const wawoff2 = require('wawoff2');

// wawoff2.compress resolves with a view into the encoder's shared WASM heap,
// so concurrent compressions overwrite each other's bytes exactly like the
// decompress side (the product-side fix is `decompressWoff2` in
// lib/font-codec). Serialise compress-and-copy here.
let compressQueue = Promise.resolve();

function compressWoff2(buffer) {
  const result = compressQueue.then(async () => {
    return Buffer.from(await wawoff2.compress(buffer));
  });
  compressQueue = result.then(
    () => undefined,
    () => undefined,
  );
  return result;
}

const cache = new Map();

// Return the WOFF2 bytes of a tiny font reporting `family` in its name table.
async function woff2Fixture(family) {
  const cached = cache.get(family);
  if (cached) {
    return cached;
  }
  const outline = new opentype.Path();
  outline.moveTo(60, 0);
  outline.lineTo(300, 700);
  outline.lineTo(540, 0);
  outline.close();
  const font = new opentype.Font({
    familyName: family,
    styleName: 'Regular',
    unitsPerEm: 1000,
    ascender: 800,
    descender: -200,
    glyphs: [
      new opentype.Glyph({ name: '.notdef', unicode: 0, advanceWidth: 600, path: new opentype.Path() }),
      new opentype.Glyph({ name: 'A', unicode: 65, advanceWidth: 600, path: outline }),
    ],
  });
  const woff2 = await compressWoff2(Buffer.from(font.toArrayBuffer()));
  cache.set(family, woff2);
  return woff2;
}

// Build a `data:font/woff2;base64,…` URL for a fixture, mirroring how the test
// corpus inlines its fonts.
async function woff2DataUrl(family) {
  const woff2 = await woff2Fixture(family);
  return 'data:font/woff2;base64,' + woff2.toString('base64');
}

module.exports = { compressWoff2, woff2Fixture, woff2DataUrl };
