// Web-font download → on-disk SFNT (TTF/OTF) export.
//
// Unlike the icon-font pass (lib/icon-font.ts), which converts individual PUA
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
// files to `pagx render --font/--fallback` or `pagx embed --fallback` so
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

import * as fs from 'fs';
import * as path from 'path';
import * as crypto from 'crypto';
import { errMessage } from './common';
import { writeFileAtomic } from './atomic-write';
import { makeCaptureListener, type CaptureLogger, type CaptureResponseListener } from './capture-listener';
import { decompressWoff2, loadFontCodec } from './font-codec';
import type { EngineName } from './browser-engine';

const fsp = fs.promises;

// Hard cap on reading a single font response body. Mirrors the icon-font
// fetch timeout — a hung CDN must not stall the whole snapshot.
const FONT_BODY_READ_TIMEOUT_MS = 10_000;

export interface FontMeta {
  family: string;
  style: string;
  ext: 'ttf' | 'otf';
}

export interface SavedFont {
  url: string;
  path: string;
  family: string;
  style: string;
  // The document's CSS family names that resolved to this file (lower-cased keys are not needed
  // here; these travel verbatim to `pagx embed --font-alias`). Empty when the file was captured
  // without a matching `@font-face` rule.
  cssFamilies: string[];
  // The full `@font-face` declarations that resolved to this file (family + style + weight).
  // `cssFamilies` is the de-duplicated family projection of this list; the face list also keeps
  // the per-face style/weight, which `injectInlineFontFaces` needs to re-declare a face without
  // letting a sibling face of the same family shadow it. Empty under the same conditions as
  // `cssFamilies`.
  cssFaces: FontFaceSource[];
}

/** A captured font file plus the CSS family it should answer to (empty when unknown). */
export interface CapturedFontRef {
  path: string;
  family: string;
}

// View a Node Buffer as a standalone ArrayBuffer slice for opentype.js
// (which only accepts ArrayBuffer, not Buffer). Slicing by byteOffset/length
// guards against Buffers that share an underlying pool with other data.
function toArrayBuffer(buffer: Buffer): ArrayBuffer {
  return buffer.buffer.slice(
    buffer.byteOffset,
    buffer.byteOffset + buffer.byteLength,
  ) as ArrayBuffer;
}

// Match the 4-byte container signatures we care about. WOFF2 = `wOF2`,
// WOFF = `wOFF`; everything else is assumed to already be a raw SFNT
// (`\0\1\0\0` TrueType, `OTTO` CFF, `true`/`ttcf`, …).
function isWoff2(buf: Buffer): boolean {
  return buf.length >= 4 && buf[0] === 0x77 && buf[1] === 0x4f && buf[2] === 0x46 && buf[3] === 0x32;
}
function isWoff(buf: Buffer): boolean {
  return buf.length >= 4 && buf[0] === 0x77 && buf[1] === 0x4f && buf[2] === 0x46 && buf[3] === 0x46;
}

// Decompress any web-font container into a plain SFNT buffer.
//   - WOFF2: decoded via the `wawoff2` WASM port of Google's reference
//     decoder, which yields the original SFNT bytes (best fidelity).
//   - WOFF:  opentype.js parses the zlib-compressed tables; we re-serialise
//     to SFNT with `font.toArrayBuffer()` so tgfx can load it.
//   - SFNT:  returned unchanged.
// WOFF2 decoding goes through `decompressWoff2`, which copies the bytes out of
// the decoder's shared WASM heap before any other decode can overwrite them.
export async function toSfnt(buffer: Buffer): Promise<Buffer> {
  const { opentype: ot } = loadFontCodec();
  if (isWoff2(buffer)) {
    return await decompressWoff2(buffer);
  }
  if (isWoff(buffer)) {
    const font = ot.parse(toArrayBuffer(buffer));
    return Buffer.from(font.toArrayBuffer());
  }
  return buffer;
}

// opentype.js name records are `{ en: '…', … }` locale maps. Prefer English,
// then any available locale, then empty.
function pickName(record: unknown): string {
  if (!record || typeof record !== 'object') return '';
  const rec = record as Record<string, unknown>;
  if (typeof rec.en === 'string' && rec.en) return rec.en;
  for (const key of Object.keys(rec)) {
    const v = rec[key];
    if (typeof v === 'string' && v) return v;
  }
  return '';
}

// Read a name-table entry by key. opentype.js v2 nests records under platform
// tables (`names.windows.fontFamily`, `names.macintosh.fontFamily`, …); v1
// exposed them flat (`names.fontFamily`). Try the platform tables first, then
// the flat layout, so the lookup works across both major versions.
function nameFrom(names: Record<string, unknown> | null | undefined, key: string): string {
  if (!names || typeof names !== 'object') return '';
  for (const platform of ['windows', 'macintosh', 'unicode']) {
    const table = (names as Record<string, Record<string, unknown> | undefined>)[platform];
    if (table && (table as Record<string, unknown>)[key]) {
      const value = pickName((table as Record<string, unknown>)[key]);
      if (value) return value;
    }
  }
  if ((names as Record<string, unknown>)[key]) {
    const value = pickName((names as Record<string, unknown>)[key]);
    if (value) return value;
  }
  return '';
}

// Read the family + sub-family names and outline flavour from an SFNT buffer.
// Used purely to build a readable, collision-free filename — PAGX matches the
// font by the family name *inside* the file at load time, so a wrong filename
// here never affects rendering, only legibility on disk.
export function readFontMeta(sfnt: Buffer): FontMeta {
  const { opentype: ot } = loadFontCodec();
  let family = '';
  let style = '';
  let isCFF = false;
  try {
    const font = ot.parse(toArrayBuffer(sfnt));
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const names = ((font as any).names || {}) as Record<string, unknown>;
    family = nameFrom(names, 'fontFamily') || nameFrom(names, 'preferredFamily');
    style = nameFrom(names, 'fontSubfamily') || nameFrom(names, 'preferredSubfamily');
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    isCFF = !!((font as any).tables && (font as any).tables.cff);
  } catch (_) {
    /* fall back to the signature sniff below */
  }
  const sig = sfnt.length >= 4 ? sfnt.slice(0, 4).toString('binary') : '';
  const ext: 'ttf' | 'otf' = isCFF || sig === 'OTTO' ? 'otf' : 'ttf';
  return { family: family || 'font', style: style || 'Regular', ext };
}

// Filesystem-safe slug. Collapses any run of disallowed characters to a
// single dash and trims leading/trailing dashes so CJK family names (which
// slug to empty) fall back to a stable default.
function sanitize(value: string): string {
  const slug = String(value).replace(/[^A-Za-z0-9._-]+/g, '-').replace(/^-+|-+$/g, '');
  return slug || 'font';
}

/** One `@font-face` source recorded from the page's CSSOM. */
export interface FontFaceSource {
  family: string;
  style: string;
  weight: string;
  url: string;
}

// Decode the payload of a `data:` URL. Returns null when `url` is not a data
// URL or its payload is malformed; both the base64 and the percent-encoded
// forms appear in real `@font-face` rules.
export function dataUrlBytes(url: string): Buffer | null {
  if (!url.startsWith('data:')) {
    return null;
  }
  const comma = url.indexOf(',');
  if (comma < 0) {
    return null;
  }
  const header = url.slice('data:'.length, comma);
  const payload = url.slice(comma + 1);
  try {
    if (/;base64/i.test(header)) {
      return Buffer.from(payload, 'base64');
    }
    return Buffer.from(decodeURIComponent(payload), 'binary');
  } catch (_err) {
    return null;
  }
}

// Decode every captured web font to SFNT and collect the document's `@font-face` declarations per
// file.
//
// Two sources feed this:
//   - `sources`, the page's `@font-face` entries. Rules whose `src` is a `data:` URL matter most:
//     Chromium issues no request for an inline font, so the response listener above can never see
//     one, and without this pass the font would neither be saved nor registered. Their bytes are
//     added to `bytesByUrl` so the regular save step writes them like any other font.
//   - Network fonts already captured in `bytesByUrl`.
//
// The returned map (cache key → authored faces) exists because a page may alias a font under a
// name that has nothing to do with the file it serves (`font-family: MaShanZheng` → the file's
// internal family is "Ma Shan Zheng"; `QingKeHuangYou` → "ZCOOL QingKe HuangYou"), and for a font
// whose typographic name (nameID 16/17) exists only in a non-English record the platform font
// reader reports it as "?????" because it can only produce ASCII. PAGX resolves text by the name
// the document carries, so the caller forwards these pairs to `pagx embed --font-alias` /
// `pagx render --font-alias`, which register the file under the authored name and bypass the
// file's own name table entirely.
//
// Every successfully decoded entry is written back into `bytesByUrl` as SFNT, so the save step
// below skips a second decode.
//
// Returns each cache key's authored `@font-face` declarations (family + style + weight), not just
// the family names: a family may ship several faces as separate inline files (a roman and an
// italic cut, say), and the writing-back step needs the style/weight to re-declare them without
// one face shadowing the other.
export async function resolveFontFamilies(
  sources: FontFaceSource[],
  bytesByUrl: Map<string, Buffer>,
  logger?: CaptureLogger | null,
): Promise<Map<string, FontFaceSource[]>> {
  const log: CaptureLogger = typeof logger === 'function' ? logger : () => {};
  const facesByKey = new Map<string, FontFaceSource[]>();
  // One payload may be declared for several families; remember its cache key so the second
  // declaration reuses the decoded entry instead of decoding again.
  const keyByUrl = new Map<string, string>();
  let inlineIndex = 0;

  for (const source of sources) {
    if (!source || !source.family || !source.url) {
      continue;
    }
    const inline = dataUrlBytes(source.url);
    let key = keyByUrl.get(source.url) || '';
    if (!key) {
      const raw = inline === null ? bytesByUrl.get(source.url) : inline;
      if (!raw || raw.length === 0) {
        continue;
      }
      key = inline === null ? source.url : `inline-font:${inlineIndex++}:${source.family}`;
      let sfnt: Buffer;
      try {
        sfnt = await toSfnt(raw);
      } catch (err) {
        log(`font decode skipped for '${source.family}': ${errMessage(err)}`);
        continue;
      }
      // Normalise the cache entry to SFNT so the save step skips a second decode.
      bytesByUrl.set(key, sfnt);
      keyByUrl.set(source.url, key);
    }
    const faces = facesByKey.get(key) || [];
    const style = String(source.style || '').trim();
    const weight = String(source.weight || '').trim();
    if (!faces.some((face) => face.family === source.family && face.style === style &&
                              face.weight === weight)) {
      faces.push({ family: source.family, style, weight, url: source.url });
    }
    facesByKey.set(key, faces);
  }
  return facesByKey;
}

// True for a cache key `resolveFontFamilies` minted for an inline (`data:`)
// source. Such a font has no URL to re-fetch and no `@font-face` rule left in
// the subset, so it is the only kind this pass writes back below.
export function isInlineFontUrl(url: string): boolean {
  return url.startsWith('inline-font:');
}

// Quote a string for a CSS `font-family` / `url()` value. Mirrors
// browser-snapshot's `cssQuotedString` so the faces this module writes back
// and the ones `collectLocalFontFaces` carry over encode names identically.
function cssQuotedString(value: string): string {
  let out = '"';
  for (const ch of String(value)) {
    if (ch === '"' || ch === '\\' || ch === '<' || ch === '>' || ch === '&' || ch < ' ') {
      out += '\\' + ch.codePointAt(0)!.toString(16) + ' ';
    } else {
      out += ch;
    }
  }
  return out + '"';
}

// Normalise a family name for comparison the way the font stack does:
// case-insensitive, trimmed, internal whitespace collapsed to one space.
function normalizeFontName(name: string): string {
  return String(name || '').trim().toLowerCase().replace(/\s+/g, ' ');
}

// Write the inline (`data:`) fonts saved during this snapshot back into the
// subset HTML as `@font-face` rules pointing at their on-disk files.
//
// A `data:` font exists only inside the original document — the browser never
// requests it, so nothing carries it into the subset: the computed
// `font-family` name survives on each text node, but the importer has no file
// to turn into a PAGX `<Font file>` resource. Text then falls back to a host
// font, which for icon fonts (Material Icons, …) means the ligature name
// itself is drawn ("location_on" instead of a pin). Re-declaring the saved
// file here closes that gap: the importer's `loadWebFontFiles` picks up local
// `@font-face` rules and emits the font resource, so the .pagx carries its
// icon fonts instead of depending on out-of-band `--font-alias`
// registrations at render time.
//
// Two restrictions keep this pass strictly additive:
//   - Network fonts stay out of it entirely: their multi-subset CJK families
//     would need `unicode-range` pairing to stay faithful, and they already
//     reach `pagx render/embed` through the font manifest.
//   - A face is only re-declared when its authored CSS name matches the
//     file's own family name. `<Font file>` resources register under the
//     font's internal family, so an aliased face (CSS "Playfair" → file
//     family "Playfair Display") could never resolve through this route —
//     writing the rule anyway would only shadow the working alias path with
//     a dead reference.
//
// Every qualifying face is emitted, carrying its own style/weight, so a
// family served as several inline files (roman + italic cuts) keeps both
// cuts distinct instead of letting the later rule shadow the earlier one.
export function injectInlineFontFaces(html: string, fonts: SavedFont[]): string {
  const rules: string[] = [];
  const seenFaces = new Set<string>();
  for (const font of fonts) {
    if (!font.path || !isInlineFontUrl(font.url)) {
      continue;
    }
    const internalFamily = normalizeFontName(font.family);
    for (const face of font.cssFaces || []) {
      if (!face.family || normalizeFontName(face.family) !== internalFamily) {
        continue;
      }
      const style = String(face.style || '').trim();
      const weight = String(face.weight || '').trim();
      const faceKey = face.family + '\n' + style + '\n' + weight;
      if (seenFaces.has(faceKey)) {
        continue;
      }
      seenFaces.add(faceKey);
      let rule = '@font-face{font-family:' + cssQuotedString(face.family);
      if (style && style !== 'normal') {
        rule += ';font-style:' + cssQuotedString(style);
      }
      if (weight && weight !== 'normal' && weight !== '400') {
        rule += ';font-weight:' + weight;
      }
      rule += ';src:url(' + cssQuotedString(font.path) + ')}';
      rules.push(rule);
    }
  }
  if (rules.length === 0) {
    return html;
  }
  const style = '<style id="__pagx_web_fonts">' + rules.join('') + '</style>';
  const headEnd = html.indexOf('</head>');
  if (headEnd < 0) {
    return html;
  }
  return html.slice(0, headEnd) + style + '\n' + html.slice(headEnd);
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
function contentFileName(meta: FontMeta, hash: string): string {
  const base = sanitize(`${meta.family}-${meta.style}`);
  return `${base}-${hash.slice(0, 16)}.${meta.ext}`;
}

// Build a `page.on('response')` listener that caches every successful font
// response body into `cache` as a `url -> Buffer` mapping. Mirrors
// snapshot-runner's image-capture listener: it never throws (a detached
// response just leaves the URL absent), and it reads the body eagerly because
// the response object may not survive past page teardown.
export function makeFontCaptureListener(
  engine: EngineName,
  cache: Map<string, Buffer>,
  log: CaptureLogger | null,
): CaptureResponseListener {
  return makeCaptureListener<Buffer>({
    resourceType: 'font',
    engine,
    cache,
    log,
    label: 'font',
    timeoutMs: FONT_BODY_READ_TIMEOUT_MS,
    timeoutMessage: 'timed out reading font body',
    skipEmptyBody: true,
  });
}

// Convert the captured `url -> Buffer` map into on-disk SFNT files under
// `outDir`. Returns `[{ url, path, family, style }]` for the unique faces
// captured. De-dupes by the decompressed SFNT's content hash so the same face
// served from multiple URLs (or already-cached re-requests) is recorded once,
// and uses a content-addressed filename so an identical face already on disk —
// e.g. written by an earlier case sharing this directory — is reused rather
// than re-written. Per-font failures are logged and skipped — a single
// undecodable payload must not lose the rest.
export async function saveDownloadedFonts(
  fontBytesByUrl: Map<string, Buffer> | null | undefined,
  outDir: string,
  logger?: CaptureLogger,
  cssFacesByKey?: Map<string, FontFaceSource[]>,
): Promise<SavedFont[]> {
  const log: CaptureLogger = typeof logger === 'function' ? logger : () => {};
  const entries = fontBytesByUrl ? Array.from(fontBytesByUrl.entries()) : [];
  if (entries.length === 0) return [];

  await fsp.mkdir(outDir, { recursive: true });

  // Decompress + hash in parallel. The WOFF2 decompression itself is serialised inside
  // `decompressWoff2` (the decoder's WASM heap is shared), so this Promise.all only overlaps the
  // hashing and name-table reads. Each entry is wrapped in its own try so a single bad font
  // doesn't poison the batch.
  type Decoded = { url: string; sfnt: Buffer; hash: string; meta: ReturnType<typeof readFontMeta> };
  const decoded = await Promise.all(entries.map(async ([url, raw]): Promise<Decoded | null> => {
    try {
      const sfnt = await toSfnt(raw);
      if (!sfnt || !sfnt.length) return null;
      const hash = crypto.createHash('sha1').update(sfnt).digest('hex');
      const meta = readFontMeta(sfnt);
      return { url, sfnt, hash, meta };
    } catch (err) {
      log(`failed to save font ${url}: ${errMessage(err)}`);
      return null;
    }
  }));

  // Dedupe by content hash sequentially so the order in which fonts win the
  // unique slot is deterministic (mirrors the previous loop's behaviour).
  // Identical bytes served under different URLs may carry different CSS faces (two subsets of one
  // family, a mirror URL), so the faces of every duplicate merge into the surviving entry.
  type Pending = {
    url: string;
    sfnt: Buffer;
    filePath: string;
    meta: ReturnType<typeof readFontMeta>;
    cssFaces: FontFaceSource[];
  };
  const pendingByHash = new Map<string, Pending>();
  for (const item of decoded) {
    if (!item) continue;
    const faces = cssFacesByKey ? cssFacesByKey.get(item.url) || [] : [];
    const existing = pendingByHash.get(item.hash);
    if (existing) {
      for (const face of faces) {
        if (!existing.cssFaces.some((f) => f.family === face.family && f.style === face.style &&
                                            f.weight === face.weight)) {
          existing.cssFaces.push(face);
        }
      }
      continue;
    }
    const fileName = contentFileName(item.meta, item.hash);
    pendingByHash.set(item.hash, {
      url: item.url,
      sfnt: item.sfnt,
      filePath: path.join(outDir, fileName),
      meta: item.meta,
      cssFaces: faces.slice(),
    });
  }

  // Content-addressed name ⇒ an existing file with this name already holds
  // the same bytes, so skip the rewrite (and the duplicate download cost on
  // disk). Otherwise stage + atomically rename so parallel cases never
  // observe a partial file.
  const results = await Promise.all(Array.from(pendingByHash.values()).map(
    async ({ url, sfnt, filePath, meta, cssFaces }) => {
      try {
        if (!fs.existsSync(filePath)) {
          await writeFileAtomic(filePath, sfnt);
        }
        const cssFamilies: string[] = [];
        for (const face of cssFaces) {
          if (!cssFamilies.includes(face.family)) {
            cssFamilies.push(face.family);
          }
        }
        return {
          url, path: filePath, family: meta.family, style: meta.style, cssFamilies, cssFaces,
        } as SavedFont;
      } catch (err) {
        log(`failed to save font ${url}: ${errMessage(err)}`);
        return null;
      }
    },
  ));
  return results.filter((r): r is SavedFont => r !== null);
}
