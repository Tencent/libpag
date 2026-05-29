// Web-font download → on-disk SFNT (TTF/OTF) export.
//
// Unlike the icon-font pass (lib/icon-font.js), which converts individual PUA
// glyphs to inline <svg> so the snapshot is self-contained, body/text web
// fonts (Google Fonts "Noto Sans SC", "Inter", self-hosted @font-face, …)
// cannot be inlined into the subset HTML: the HTML subset spec disallows
// `@font-face` and the importer drops it. The snapshot only records the
// computed `font-family` *name* on each text node, so at render time PAGX
// resolves the family via `tgfx::Typeface::MakeFromName` — which only finds
// fonts installed on the host. A page styled with a downloaded web font that
// isn't installed locally therefore renders with a wrong/fallback face.
//
// This module bridges that gap by saving the actual font *files* the browser
// already downloaded while rendering the page. The caller can then hand those
// files to `pagx render --font/--fallback` or `pagx font embed --fallback` so
// the document renders (or embeds) with the correct typeface.
//
// Why capture the browser's downloads instead of parsing @font-face URLs:
//   - Chromium only fetches the @font-face `src` it actually uses, including
//     — crucially for CJK — only the unicode-range subset files the page's
//     text falls into. A Google-Fonts "Noto Sans SC" stylesheet declares 100+
//     subset @font-face blocks per weight; the browser pulls the handful that
//     cover the page. Re-deriving that set from CSS would mean downloading
//     everything.
//   - It sidesteps CORS on the font binaries (the browser already has the
//     bytes; we read them off the response).
//
// Pipeline:
//   1. snapshot-runner registers `makeFontCaptureListener` on `page` before
//      navigation; every `resourceType() === 'font'` response body is cached
//      by URL.
//   2. After the page settles, `saveDownloadedFonts` decompresses each WOFF2 /
//      WOFF payload to a plain SFNT (tgfx::Typeface::MakeFromPath only reads
//      SFNT containers), parses the family/sub-family from the font's `name`
//      table for a human-readable filename, de-dupes by content hash, and
//      writes the files into the requested directory.

'use strict';

const fs = require('fs');
const fsp = require('fs').promises;
const path = require('path');
const crypto = require('crypto');
const { responseBytes } = require('./browser-engine');
const { errMessage } = require('./cli');

let opentype = null;
let wawoff2 = null;
function loadDeps() {
  if (!opentype) opentype = require('opentype.js');
  if (!wawoff2) wawoff2 = require('wawoff2');
  return { opentype, wawoff2 };
}

// Hard cap on reading a single font response body. Mirrors the icon-font
// fetch timeout — a hung CDN must not stall the whole snapshot.
const FONT_BODY_READ_TIMEOUT_MS = 10_000;

// View a Node Buffer as a standalone ArrayBuffer slice for opentype.js
// (which only accepts ArrayBuffer, not Buffer). Slicing by byteOffset/length
// guards against Buffers that share an underlying pool with other data.
function toArrayBuffer(buffer) {
  return buffer.buffer.slice(buffer.byteOffset, buffer.byteOffset + buffer.byteLength);
}

// Match the 4-byte container signatures we care about. WOFF2 = `wOF2`,
// WOFF = `wOFF`; everything else is assumed to already be a raw SFNT
// (`\0\1\0\0` TrueType, `OTTO` CFF, `true`/`ttcf`, …).
function isWoff2(buf) {
  return buf.length >= 4 && buf[0] === 0x77 && buf[1] === 0x4f && buf[2] === 0x46 && buf[3] === 0x32;
}
function isWoff(buf) {
  return buf.length >= 4 && buf[0] === 0x77 && buf[1] === 0x4f && buf[2] === 0x46 && buf[3] === 0x46;
}

// Decompress any web-font container into a plain SFNT buffer.
//   - WOFF2: decoded via the `wawoff2` WASM port of Google's reference
//     decoder, which yields the original SFNT bytes (best fidelity).
//   - WOFF:  opentype.js parses the zlib-compressed tables; we re-serialise
//     to SFNT with `font.toArrayBuffer()` so tgfx can load it.
//   - SFNT:  returned unchanged.
async function toSfnt(buffer) {
  const { opentype: ot, wawoff2: w2 } = loadDeps();
  if (isWoff2(buffer)) {
    const decoded = await w2.decompress(buffer);
    return Buffer.isBuffer(decoded) ? decoded : Buffer.from(decoded);
  }
  if (isWoff(buffer)) {
    const font = ot.parse(toArrayBuffer(buffer));
    return Buffer.from(font.toArrayBuffer());
  }
  return buffer;
}

// opentype.js name records are `{ en: '…', … }` locale maps. Prefer English,
// then any available locale, then empty.
function pickName(record) {
  if (!record || typeof record !== 'object') return '';
  if (typeof record.en === 'string' && record.en) return record.en;
  for (const key of Object.keys(record)) {
    if (typeof record[key] === 'string' && record[key]) return record[key];
  }
  return '';
}

// Read a name-table entry by key. opentype.js v2 nests records under platform
// tables (`names.windows.fontFamily`, `names.macintosh.fontFamily`, …); v1
// exposed them flat (`names.fontFamily`). Try the platform tables first, then
// the flat layout, so the lookup works across both major versions.
function nameFrom(names, key) {
  if (!names || typeof names !== 'object') return '';
  for (const platform of ['windows', 'macintosh', 'unicode']) {
    const table = names[platform];
    if (table && table[key]) {
      const value = pickName(table[key]);
      if (value) return value;
    }
  }
  if (names[key]) {
    const value = pickName(names[key]);
    if (value) return value;
  }
  return '';
}

// Read the family + sub-family names and outline flavour from an SFNT buffer.
// Used purely to build a readable, collision-free filename — PAGX matches the
// font by the family name *inside* the file at load time, so a wrong filename
// here never affects rendering, only legibility on disk.
function readFontMeta(sfnt) {
  const { opentype: ot } = loadDeps();
  let family = '';
  let style = '';
  let isCFF = false;
  try {
    const font = ot.parse(toArrayBuffer(sfnt));
    const names = font.names || {};
    family = nameFrom(names, 'fontFamily') || nameFrom(names, 'preferredFamily');
    style = nameFrom(names, 'fontSubfamily') || nameFrom(names, 'preferredSubfamily');
    isCFF = !!(font.tables && font.tables.cff);
  } catch (_) {
    /* fall back to the signature sniff below */
  }
  const sig = sfnt.length >= 4 ? sfnt.slice(0, 4).toString('binary') : '';
  const ext = isCFF || sig === 'OTTO' ? 'otf' : 'ttf';
  return { family: family || 'font', style: style || 'Regular', ext };
}

// Filesystem-safe slug. Collapses any run of disallowed characters to a
// single dash and trims leading/trailing dashes so CJK family names (which
// slug to empty) fall back to a stable default.
function sanitize(value) {
  const slug = String(value).replace(/[^A-Za-z0-9._-]+/g, '-').replace(/^-+|-+$/g, '');
  return slug || 'font';
}

// Build a content-addressed `<Family>-<Style>-<hash>.<ext>` filename. The
// hash suffix (first 16 hex chars of the SFNT's SHA-1) makes the name a pure
// function of the bytes, which gives us two properties for free:
//   - De-duplication: the same face downloaded by multiple pages/cases — even
//     across separate snapshot runs sharing one font directory — always maps
//     to the same filename, so it is stored once instead of accumulating
//     `-1`, `-2`, … copies of identical bytes.
//   - Coverage: CJK/latin web fonts arrive as many unicode-range subsets that
//     share one family+style but differ in content; their distinct hashes keep
//     each subset as its own file, so the glyph coverage is preserved.
function contentFileName(meta, hash) {
  const base = sanitize(`${meta.family}-${meta.style}`);
  return `${base}-${hash.slice(0, 16)}.${meta.ext}`;
}

// Write `data` to `filePath` without ever exposing a half-written file to a
// concurrent reader (eval/run.js processes many cases in parallel into one
// shared font directory). We stage the bytes in a per-call temp file and
// atomically rename it into place; if another worker won the race and created
// the (byte-identical, content-addressed) target first, we discard our temp
// and treat it as success.
async function writeFileAtomic(filePath, data) {
  const tmp = `${filePath}.tmp-${process.pid}-${crypto.randomBytes(4).toString('hex')}`;
  await fsp.writeFile(tmp, data);
  try {
    await fsp.rename(tmp, filePath);
  } catch (err) {
    await fsp.unlink(tmp).catch(() => {});
    if (!fs.existsSync(filePath)) throw err;
  }
}

// Build a `page.on('response')` listener that caches every successful font
// response body into `cache` as a `url -> Buffer` mapping. Mirrors
// snapshot-runner's image-capture listener: it never throws (a detached
// response just leaves the URL absent), and it reads the body eagerly because
// the response object may not survive past page teardown.
function makeFontCaptureListener(engine, cache, log) {
  return async function captureFontResponse(resp) {
    try {
      const req = resp.request();
      if (req.resourceType() !== 'font') return;
      const url = req.url();
      if (!url || url.startsWith('data:')) return;
      if (!resp.ok()) return;
      if (cache.has(url)) return;
      const buf = await Promise.race([
        responseBytes(resp, engine),
        new Promise((_, reject) =>
          setTimeout(() => reject(new Error('timed out reading font body')), FONT_BODY_READ_TIMEOUT_MS),
        ),
      ]);
      if (buf && buf.length) cache.set(url, buf);
    } catch (err) {
      if (log) log(`font capture skipped: ${errMessage(err)}`);
    }
  };
}

// Convert the captured `url -> Buffer` map into on-disk SFNT files under
// `outDir`. Returns `[{ url, path, family, style }]` for the unique faces
// captured. De-dupes by the decompressed SFNT's content hash so the same face
// served from multiple URLs (or already-cached re-requests) is recorded once,
// and uses a content-addressed filename so an identical face already on disk —
// e.g. written by an earlier case sharing this directory — is reused rather
// than re-written. Per-font failures are logged and skipped — a single
// undecodable payload must not lose the rest.
async function saveDownloadedFonts(fontBytesByUrl, outDir, logger) {
  const log = typeof logger === 'function' ? logger : () => {};
  const entries = fontBytesByUrl ? Array.from(fontBytesByUrl.entries()) : [];
  if (entries.length === 0) return [];

  await fsp.mkdir(outDir, { recursive: true });

  const seenHashes = new Set();
  const saved = [];
  for (const [url, raw] of entries) {
    try {
      const sfnt = await toSfnt(raw);
      if (!sfnt || !sfnt.length) continue;
      const hash = crypto.createHash('sha1').update(sfnt).digest('hex');
      if (seenHashes.has(hash)) continue;
      seenHashes.add(hash);
      const meta = readFontMeta(sfnt);
      const fileName = contentFileName(meta, hash);
      const filePath = path.join(outDir, fileName);
      // Content-addressed name ⇒ an existing file with this name already holds
      // the same bytes, so skip the rewrite (and the duplicate download cost
      // on disk). Otherwise stage + atomically rename so parallel cases never
      // observe a partial file.
      if (!fs.existsSync(filePath)) {
        await writeFileAtomic(filePath, sfnt);
      }
      saved.push({ url, path: filePath, family: meta.family, style: meta.style });
    } catch (err) {
      log(`failed to save font ${url}: ${errMessage(err)}`);
    }
  }
  return saved;
}

module.exports = {
  makeFontCaptureListener,
  saveDownloadedFonts,
  // Exported for unit-level reuse / testing.
  toSfnt,
  readFontMeta,
};
