// Lazy loader for the two WASM/parser dependencies that turn a captured web
// font byte stream into something we can introspect: `opentype.js` (parses
// TTF/OTF and re-serialises WOFF -> SFNT) and `wawoff2` (decompresses
// WOFF2 to SFNT).
//
// Why this lives in its own module: lib/icon-font.ts and lib/font-download.ts
// both kept their own copy of the same `loadDeps()`:
//
//     let opentype = null;
//     let wawoff2 = null;
//     function loadDeps() {
//       if (!opentype) opentype = require('opentype.js');
//       if (!wawoff2)  wawoff2  = require('wawoff2');
//       return { opentype, wawoff2 };
//     }
//
// The pattern's purpose is "do not pay the wawoff2 wasm decode cost on
// startup if the run never touches a web font" — important for `server.js`
// where most requests are local HTML files. Centralising the loader means
// only one module-level cache, and any future codec change (e.g. swapping
// `wawoff2` for `wawoff2-sync`) lands in one place.
//
// Why not `require()` the two packages eagerly at module load: `wawoff2`
// instantiates a multi-MB WebAssembly module the first time it is imported.
// Lazy-requiring it postpones that cost to the first font we actually need
// to decode; pages without web fonts pay nothing.

// `opentype.js` and `wawoff2` are typed loosely on purpose — the icon-font
// path uses a small slice of opentype's API and there are no published types
// for `wawoff2`.
type OpenTypeModule = typeof import('opentype.js');
// eslint-disable-next-line @typescript-eslint/no-explicit-any
type Wawoff2Module = any;

let opentype: OpenTypeModule | null = null;
let wawoff2: Wawoff2Module | null = null;

export interface FontCodecBundle {
  opentype: OpenTypeModule;
  wawoff2: Wawoff2Module;
}

// Return the two libraries, requiring each one on first call. Subsequent
// calls reuse the cached references — Node's require cache also caches the
// module, so the lazy guard is only here to defer the initial load, not to
// outsmart the require cache.
export function loadFontCodec(): FontCodecBundle {
  if (!opentype) opentype = require('opentype.js') as OpenTypeModule;
  if (!wawoff2) wawoff2 = require('wawoff2') as Wawoff2Module;
  return { opentype, wawoff2 };
}

// Serialises every WOFF2 decode in this process. See `decompressWoff2`.
let woff2Queue: Promise<unknown> = Promise.resolve();

// Decode a WOFF2 payload into raw SFNT bytes.
//
// `wawoff2.decompress` resolves with a `Uint8Array` that aliases the decoder's
// module-owned WASM heap: the next decode overwrites those bytes in place.
// The wrapping async function yields between the resolve and the caller's
// copy, so two decodes started under one `Promise.all` interleave exactly
// that way — each receives the *last* call's output and every earlier font
// silently corrupts into an unparseable name table. Funnel every decode
// through this queue so the copy completes before the next decode can run.
export function decompressWoff2(buffer: Buffer): Promise<Buffer> {
  const result = woff2Queue.then(async () => {
    const { wawoff2 } = loadFontCodec();
    const decoded = await wawoff2.decompress(buffer);
    return Buffer.from(decoded);
  });
  // Advance the queue on both settle paths so a failed decode cannot wedge
  // every later font behind a rejected chain link.
  woff2Queue = result.then(
    () => undefined,
    () => undefined,
  );
  return result;
}
