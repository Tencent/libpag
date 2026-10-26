// Copyright (C) 2026 Tencent. All rights reserved.
// @ts-nocheck

// ===== Pre-snapshot pass: inline external <img> sources =====

// Walk every <img> in the document and normalise its `src` for `pagx render`,
// which can read `data:` URIs and local files but cannot fetch URLs at render
// time. The result is stashed on `data-snapshot-src`, which the downstream
// snapshot reader (`imgSrc`) prefers over the original attribute:
//
//   - `http(s)://` src  → a `data:` URI of the bytes (self-contained snapshot).
//   - `file://` src      → the absolute filesystem path. A relative <img src>
//     on a local page resolves to a `file://` URL the browser computed against
//     the source document; emitting it as an absolute path keeps the subset
//     valid regardless of where it is written, since PAGX otherwise resolves a
//     bare relative `src` against the subset HTML's own directory.
//
// Two byte sources, in priority order:
//
//   1. `cachedMap` — a `{ url: dataURI }` table populated by snapshot.js
//      from `page.on('response')`. The browser already loaded these images
//      to render the page, so reusing those bytes bypasses the second
//      cross-origin round-trip entirely (and the CORS preflight that the
//      old `fetch(url, { mode: 'cors' })` path silently failed at — most
//      business image CDNs do not return ACAO headers, so the legacy path
//      lost essentially every cross-origin image).
//
//   2. In-page `fetch()` fallback — used when the cache misses (e.g. the
//      browser-bundle entry point in build-browser-bundle.js, which has no
//      Node side to capture responses; or images that loaded before the
//      response listener attached). Limited to 8 concurrent requests so
//      pages with hundreds of icons don't trip same-origin connection
//      caps or upstream 429 throttling.
//
// Failures (network, 404, tainted cache hit) are logged and left in place:
// the snapshot will then fall back to the original URL and render as an
// empty box rather than aborting the pipeline.
async function inlineExternalImages(cachedMap) {
  // Convert a Blob into a `data:` URI without the FileReader callback dance.
  // Reading bytes via `arrayBuffer()` + chunked btoa avoids the "Maximum
  // call stack size" trap that String.fromCharCode(...big) hits on large
  // images.
  async function blobToDataUri(blob) {
    const buf = await blob.arrayBuffer();
    const bytes = new Uint8Array(buf);
    let binary = '';
    const CHUNK = 0x8000;
    for (let i = 0; i < bytes.length; i += CHUNK) {
      binary += String.fromCharCode.apply(null, bytes.subarray(i, i + CHUNK));
    }
    const mime = blob.type || 'application/octet-stream';
    return `data:${mime};base64,${btoa(binary)}`;
  }

  // Formats `<Image>` is allowed to carry, plus SVG (which the HTML importer routes to native
  // vector nodes instead of a raster). Everything else has to be re-encoded: PAGX consumers are
  // only required to decode PNG/JPEG/WebP/GIF, so an AVIF or HEIC that a CDN served would render
  // on the machine that happens to have the codec and silently vanish everywhere else — Windows
  // ships no AV1 decoder by default, and a design tool that reads the file with a narrow codec
  // set drops it outright.
  const PASSTHROUGH_MIMES = [
    'image/png',
    'image/jpeg',
    'image/jpg',
    'image/webp',
    'image/gif',
    'image/svg+xml',
  ];

  // WebP keeps alpha and is in the PAGX supported set with a software decoder in the engine, so it
  // is the one target that every consumer can read. Quality is high enough that a lossy re-encode
  // of an already-lossy source is not visible at the sizes these images are composited at.
  const WEBP_QUALITY = 0.92;

  // Media type declared by a `data:` URI, lower-cased and without parameters. Empty when the URI
  // declares none (`data:;base64,…`), which is treated as "not known to be supported".
  function dataUriMime(src) {
    const match = /^data:([^;,]+)/i.exec(src);
    return match ? match[1].toLowerCase() : '';
  }

  // Chromium is the only image codec available on this side of the pipeline (the Node side carries
  // no AVIF/HEIC decoder), so the re-encode happens here: decode the blob, draw it 1:1 and let the
  // browser's own WebP encoder produce the replacement bytes.
  async function transcodeToWebpDataUri(blob) {
    const bitmap = await createImageBitmap(blob);
    try {
      const canvas = new OffscreenCanvas(bitmap.width, bitmap.height);
      canvas.getContext('2d').drawImage(bitmap, 0, 0);
      const encoded = await canvas.convertToBlob({ type: 'image/webp', quality: WEBP_QUALITY });
      // A browser without a WebP encoder hands back a PNG blob; labelling that as WebP would lie
      // about the payload, so treat it as a failure and keep the original bytes.
      if (encoded.type !== 'image/webp') {
        throw new Error('no webp encoder');
      }
      return await blobToDataUri(encoded);
    } finally {
      // The bitmap owns a decoded copy of the image; release it explicitly instead of leaving a
      // page full of large images to the garbage collector.
      bitmap.close();
    }
  }

  // Normalises the bytes of one fetched blob for the snapshot. A format PAGX can carry is inlined
  // as-is; anything else is replaced by its WebP re-encode. Re-encoding failures keep the original
  // bytes and warn — a kept AVIF still renders wherever the codec exists, whereas dropping the
  // image would lose it everywhere.
  async function blobToNormalizedDataUri(blob) {
    const mime = (blob.type || '').split(';')[0].trim().toLowerCase();
    if (PASSTHROUGH_MIMES.indexOf(mime) >= 0) {
      return await blobToDataUri(blob);
    }
    try {
      return await transcodeToWebpDataUri(blob);
    } catch (err) {
      console.warn(
        `html-snapshot: keeping ${mime || 'unlabelled'} image as-is (${err && err.message})`,
      );
      return await blobToDataUri(blob);
    }
  }

  // Normalises an already-resolved source string: a data URI whose format PAGX cannot carry is
  // re-encoded, while a supported format is returned unchanged. Two forms are left alone by
  // design: a remote URL (the callers fetch those separately) and a filesystem path — the shape
  // `--download-images` passes, whose bytes never reach the browser, so the on-disk file keeps
  // whatever format the server sent. That mode is a capture artifact rather than the input the
  // PAGX pipeline consumes, so it is not re-encoded.
  async function normalizeSource(src) {
    if (!src.startsWith('data:')) return src;
    if (PASSTHROUGH_MIMES.indexOf(dataUriMime(src)) >= 0) return src;
    try {
      const res = await fetch(src);
      return await blobToNormalizedDataUri(await res.blob());
    } catch (err) {
      const mime = dataUriMime(src) || 'unlabelled';
      console.warn(`html-snapshot: keeping ${mime} image as-is (${err && err.message})`);
      return src;
    }
  }

  // Convert a `file://` URL (the absolute form the browser resolves a local /
  // relative <img src> into) back to a plain filesystem path. PAGX's importer
  // treats a leading `/` (POSIX) or `C:/` (Windows) as absolute and reads the
  // file directly, but does not understand the `file://` scheme, so we strip
  // it and percent-decode the path. Returns '' for anything that isn't a
  // file URL so the caller can fall through to its other branches.
  function fileUrlToPath(fileUrl) {
    try {
      const u = new URL(fileUrl);
      if (u.protocol !== 'file:') return '';
      let p = decodeURIComponent(u.pathname);
      // Windows: `file:///C:/dir/x.png` → pathname `/C:/dir/x.png`; drop the
      // leading slash so the drive letter starts the path.
      if (/^\/[A-Za-z]:/.test(p)) p = p.slice(1);
      return p;
    } catch (_) {
      return '';
    }
  }

  const cache = cachedMap || {};
  const imgs = Array.from(document.querySelectorAll('img'));
  // Resource pixels and CSS natural size are different for density-selected sources. Read
  // selected resources without srcset once, using the browser's decoded-resource cache.
  const pixelSizes = new Map();
  await Promise.all(imgs.map(async img => {
    if (!(img.getAttribute('srcset') || '').trim() && img.parentElement?.tagName !== 'PICTURE') return;
    const selected = img.currentSrc;
    if (!selected || !img.naturalWidth) return;
    if (!pixelSizes.has(selected)) pixelSizes.set(selected, (async () => {
      const probe = new Image();
      probe.src = selected;
      try { await probe.decode(); return probe.naturalWidth; } catch (_) { return 0; }
    })());
    const width = await pixelSizes.get(selected);
    if (width > 0) img.setAttribute('data-snapshot-image-pixel-width', String(width));
  }));
  // `pending` needs a fetch; `inlinePending` already holds its bytes (a response-cache hit, or a
  // data URI the page authored) and only needs normalising.
  const pending = [];
  const inlinePending = [];
  for (const img of imgs) {
    // A placeholder <img> whose `src`/`srcset` attributes are empty or missing
    // is not a real image, but the IDL getters below (`currentSrc` / `img.src`)
    // resolve an empty `src=""` against the document base URL — i.e. they hand
    // back the *page's own URL* (e.g. `file://.../slide-3.html`). Left
    // unchecked, that self-URL gets stamped onto `data-snapshot-src` and the
    // downstream importer emits an `<Image source="....html">` pointing at the
    // page itself. Guard on the raw attributes so these placeholders are
    // skipped outright.
    const rawSrc = (img.getAttribute('src') || '').trim();
    const rawSrcset = (img.getAttribute('srcset') || '').trim();
    // A <picture> may select its source from a <source srcset> only, leaving
    // both attributes on the <img> empty while `currentSrc` is real.
    const pictureSource = img.parentElement?.tagName === 'PICTURE' && img.currentSrc;
    if (!rawSrc && !rawSrcset && !pictureSource) continue;
    // Skip images an earlier pass already inlined (a `data:` URI or local path
    // stamped on `data-snapshot-src`). This function may run more than once on
    // a JS-driven page — components can mount fresh <img>s after the first pass
    // (e.g. once the animation-capture virtual clock advances) — so this guard
    // keeps the repeat pass from needlessly re-fetching every already-inlined
    // image and only lets the newly mounted ones fall through.
    if ((img.getAttribute('data-snapshot-src') || '').trim()) continue;
    const src = img.currentSrc || img.src || img.getAttribute('src') || '';
    if (!src) continue;
    if (src.startsWith('data:')) {
      // A page that inlines its own images still has to be normalised: the data URI may carry a
      // format PAGX cannot (an AVIF the author embedded, for instance), and no later stage would
      // re-encode it. A supported format is returned unchanged, so this costs a string compare.
      inlinePending.push({ img, src });
      continue;
    }
    // Local image referenced by a relative (or absolute-but-relative-to-the-
    // source) path: the browser has already resolved it to an absolute
    // `file://` URL. Emit that as a plain absolute filesystem path so the
    // subset HTML keeps working no matter which directory it is written to —
    // PAGX otherwise resolves a bare relative `src` against the subset's own
    // location, which breaks the moment the subset and the image files no
    // longer share a directory (e.g. the eval pipeline writes the subset under
    // out/<label>/<case>/ while the images live elsewhere).
    if (/^file:/i.test(src)) {
      const fsPath = fileUrlToPath(src);
      if (fsPath) img.setAttribute('data-snapshot-src', fsPath);
      continue;
    }
    if (!/^https?:/i.test(src)) continue;
    const cached = cache[src];
    if (cached) {
      // Re-encoding an unsupported format needs the browser's codec, so it runs in the same
      // chunked pass as the fetches below instead of one await per element here.
      inlinePending.push({ img, src: cached });
      continue;
    }
    pending.push({ img, src });
  }

  async function processOne(entry) {
    try {
      const res = await fetch(entry.src, { credentials: 'omit' });
      if (!res.ok) {
        console.warn(`html-snapshot: image fetch ${res.status} for ${entry.src}`);
        return;
      }
      const dataUri = await blobToNormalizedDataUri(await res.blob());
      entry.img.setAttribute('data-snapshot-src', dataUri);
    } catch (err) {
      console.warn(`html-snapshot: failed to inline ${entry.src}: ${err && err.message}`);
    }
  }

  // Same-origin browsers cap parallel connections at ~6, and many CDNs
  // throttle aggressively past that. Keeping the in-flight window at 8
  // lets us saturate the bottleneck without queueing surprises.
  const FETCH_CHUNK_SIZE = 8;
  async function applyInlineSource(entry) {
    entry.img.setAttribute('data-snapshot-src', await normalizeSource(entry.src));
  }
  for (let i = 0; i < inlinePending.length; i += FETCH_CHUNK_SIZE) {
    const slice = inlinePending.slice(i, i + FETCH_CHUNK_SIZE);
    await Promise.all(slice.map(applyInlineSource));
  }
  for (let i = 0; i < pending.length; i += FETCH_CHUNK_SIZE) {
    const slice = pending.slice(i, i + FETCH_CHUNK_SIZE);
    await Promise.all(slice.map(processOne));
  }

  // Pass 2: remote CSS `background-image: url(...)`. Chromium reports the url already resolved
  // to an absolute form. Local `file://` urls are handled at emit time by
  // `normalizeBackgroundImage` (stripped to a plain path the importer loads directly), so only
  // remote `http(s)` bytes need inlining here — fetch them and overwrite the element's inline
  // `background-image` with a `data:` URI so the walker emits a self-contained reference.
  function firstCssUrl(value) {
    const m = /url\(\s*(['"]?)([^'")]+)\1\s*\)/i.exec(value || '');
    return m ? m[2] : '';
  }

  // A stack that mixes a colour wash with a photo — `linear-gradient(...), url(photo.jpg)`, the
  // usual "tinted card" pattern — is inlined layer by layer. Replacing the whole declaration
  // would throw the gradient away, and leaving the photo remote makes the importer emit an
  // `<Image source="https://…">` the renderer cannot fetch, so the card renders as bare text.
  // Only a single-url stack is handled: `url(...), url(...)` has no gradient to preserve and
  // keeps the previous whole-declaration replacement.
  function hasGradientLayer(value) {
    return /(?:^|\s|,)(?:repeating-)?(?:linear|radial|conic)-gradient\(/i.test(value || '');
  }

  // Replaces just the `url(...)` token, leaving every other background layer in place. The data
  // URI is emitted single-quoted because `normalizeBackgroundImage` passes a gradient-bearing
  // value through verbatim: the value lands inside the double-quoted `style="…"` attribute the
  // walker writes, where a double-quoted url() would close the attribute early.
  function replaceFirstCssUrl(value, replacement) {
    const token = `url('${replacement.replace(/\\/g, '\\\\').replace(/'/g, "\\'")}')`;
    return value.replace(/url\(\s*(['"]?)([^'")]+)\1\s*\)/i, () => token);
  }

  const bgPending = [];
  const bgInlinePending = [];
  const allEls = Array.from(document.querySelectorAll('*'));
  for (const el of allEls) {
    const bg = getComputedStyle(el).getPropertyValue('background-image').trim();
    const layered = hasGradientLayer(bg) && (bg.match(/url\(/gi) || []).length === 1;
    if (!bg || (!/^url\(/i.test(bg) && !layered)) continue;
    const url = firstCssUrl(bg);
    if (!url || /^file:/i.test(url)) continue;
    if (url.startsWith('data:')) {
      bgInlinePending.push({ el, src: url, current: url, bg, layered });
      continue;
    }
    if (!/^https?:/i.test(url)) continue;
    const cached = cache[url];
    if (cached) {
      // `current` records what the element still declares: a cache hit has to be written back even
      // when the cached bytes already are in a portable format, because the element is carrying
      // the remote URL that entry replaces.
      bgInlinePending.push({ el, src: cached, current: url, bg, layered });
      continue;
    }
    bgPending.push({ el, url, bg, layered });
  }

  // The inline declaration a fetched/cached url layer is written back into: a layered stack gets
  // only its url token swapped, anything else keeps the whole-declaration replacement.
  function writeInlinedBackground(entry, value) {
    entry.el.style.backgroundImage = entry.layered
      ? replaceFirstCssUrl(entry.bg, value)
      : `url("${value.replace(/"/g, '\\"')}")`;
  }

  async function processOneBg(entry) {
    try {
      const res = await fetch(entry.url, { credentials: 'omit' });
      if (!res.ok) {
        console.warn(`html-snapshot: bg-image fetch ${res.status} for ${entry.url}`);
        return;
      }
      writeInlinedBackground(entry, await blobToNormalizedDataUri(await res.blob()));
    } catch (err) {
      console.warn(`html-snapshot: failed to inline bg ${entry.urls.join(', ')}: ${err && err.message}`);
    }
  }

  async function applyInlineBgSource(entry) {
    const normalized = await normalizeSource(entry.src);
    // Skip the write only when the element already declares the value it would receive: an
    // authored data URI that needed no re-encode stays untouched, so a page full of icon data
    // URIs does not pay for a pointless style invalidation per element. Everything else — a cache
    // hit above all — has to replace whatever the element declares now.
    if (normalized !== entry.current) {
      writeInlinedBackground(entry, normalized);
    }
  }
  for (let i = 0; i < bgInlinePending.length; i += FETCH_CHUNK_SIZE) {
    const slice = bgInlinePending.slice(i, i + FETCH_CHUNK_SIZE);
    await Promise.all(slice.map(applyInlineBgSource));
  }
  for (let i = 0; i < bgPending.length; i += FETCH_CHUNK_SIZE) {
    const slice = bgPending.slice(i, i + FETCH_CHUNK_SIZE);
    await Promise.all(slice.map(processOneBg));
  }

  // Top-level comma split of a CSS value list. Commas nested inside `rgb(...)`,
  // `linear-gradient(...)` or a quoted `url('…')` argument — a `data:` URI in particular — must
  // not split the list. Mirrors the importer's `SplitTopLevelCommas`.
  function splitTopLevelCommas(value) {
    const out = [];
    let depth = 0;
    let cur = '';
    for (const ch of String(value || '')) {
      if (ch === '(') depth++;
      else if (ch === ')') depth = Math.max(0, depth - 1);
      if (ch === ',' && depth === 0) {
        if (cur.trim() !== '') out.push(cur.trim());
        cur = '';
        continue;
      }
      cur += ch;
    }
    if (cur.trim() !== '') out.push(cur.trim());
    return out;
  }

  // A stack that layers a colour wash over a photo cannot be expressed as one PAGX background:
  // the importer paints either the gradient layers or the image, never both. Split it into two
  // boxes that the existing paths already handle — the element keeps the photo as a lone url()
  // background (so its `background-size` / `-position` / `-repeat` apply as usual), and a
  // synthesized full-bleed child carries the wash on top of it. Only backdrop boxes are split:
  // an element with content of its own would need the wash slotted between its background and
  // that content, which the flattened subset has no way to express.
  function splitTintedBackgrounds() {
    for (const el of Array.from(document.querySelectorAll('*'))) {
      const computed = getComputedStyle(el);
      const bg = (computed.getPropertyValue('background-image') || '').trim();
      const layers = splitTopLevelCommas(bg);
      if (!hasGradientLayer(bg)) {
        // A stack of two url() layers cannot be expressed as one PAGX background: the importer
        // reads a single source per element, so the whole declaration would resolve to only its
        // first layer. Keep the bottom layer on the element and peel the top one into a
        // synthesized full-bleed child — the same shape as the wash split below. Only the exact
        // two-layer, unblended, default-clip form is split; every other stack keeps the
        // declaration, which the importer now resolves to its first layer.
        //
        // Unlike the wash split this branch also handles boxes with content of their own: the
        // synthesized child is inserted *ahead* of the existing children, so the replay paints
        // the element background, then the top image layer, then the content — exactly how CSS
        // stacks every background layer beneath content. The one mismatch a full-bleed child
        // would introduce is the corner clip, so it inherits the host's `border-radius`.
        if (layers.length !== 2 || !/^url\(/i.test(layers[0]) || !/^url\(/i.test(layers[1])) {
          continue;
        }
        const blendModes = splitTopLevelCommas(
          computed.getPropertyValue('background-blend-mode') || 'normal');
        if (blendModes.some((mode) => {
          const m = mode.trim().toLowerCase();
          return m !== '' && m !== 'normal';
        })) {
          continue;
        }
        const clips = splitTopLevelCommas(
          computed.getPropertyValue('background-clip') || 'border-box');
        if (clips.some((clip) => {
          const c = clip.trim().toLowerCase();
          return c !== '' && c !== 'border-box';
        })) {
          continue;
        }
        // The synthesized child's absolute inset resolves against the host, so a static host
        // (the usual grid-tile shape — the markup carries only the background-image) must
        // become positioned first. Promoting a static box to `relative` is otherwise a no-op
        // for live layout, but it does re-anchor any absolutely-positioned descendant whose
        // containing block currently sits further up the tree — bail on those instead of
        // silently moving them.
        if (computed.position === 'static') {
          const reanchorsDescendant = Array.from(el.querySelectorAll('*')).some(
            (descendant) => getComputedStyle(descendant).position === 'absolute');
          if (reanchorsDescendant) continue;
          el.style.position = 'relative';
        }
        // Each fitting property is a per-layer list; the element now carries a single layer, so
        // it needs that layer's own entry rather than the list. A one-entry value is already
        // shared by every layer and passes through unchanged. The synthesized top child needs
        // the CSS-topmost layer's entry instead.
        const pickLayer = (value, index, fallback) => {
          const entries = splitTopLevelCommas(value || '');
          const picked = entries.length > 1 ? entries[index] : entries[0];
          return picked || fallback;
        };
        el.style.backgroundImage = layers[1];
        el.style.backgroundSize = pickLayer(computed.getPropertyValue('background-size'), 1, 'auto');
        el.style.backgroundPosition = pickLayer(
          computed.getPropertyValue('background-position'), 1, '0% 0%');
        el.style.backgroundRepeat = pickLayer(
          computed.getPropertyValue('background-repeat'), 1, 'repeat');
        const top = document.createElement('div');
        top.setAttribute('data-pagx-tint', '');
        top.setAttribute(
          'style',
          // The synthesized child matches the page's container rules (`.tile div { … }` shapes
          // are common), so its own computed background/border/shadow would replay onto the
          // layer and paint over the host's lower image. Pin those back to the no-op values.
          // `z-index: -1` pins the layer into the walker's negative paint band so it replays
          // ahead of the host's content — a plain `auto` z-index sorts it *after* the content
          // (whose positioning comes from the authored CSS), painting the image over the very
          // text the stack was supposed to sit behind.
          'position:absolute;left:0;top:0;width:100%;height:100%;z-index:-1;border-radius:inherit;' +
            'background-color:transparent;border:0;box-shadow:none;' +
            'background-image:' +
            layers[0] +
            ';background-size:' +
            pickLayer(computed.getPropertyValue('background-size'), 0, 'auto') +
            ';background-position:' +
            pickLayer(computed.getPropertyValue('background-position'), 0, '0% 0%') +
            ';background-repeat:' +
            pickLayer(computed.getPropertyValue('background-repeat'), 0, 'repeat') +
            ';'
        );
        el.insertBefore(top, el.firstChild);
        continue;
      }
      // The wash split stays backdrop-only: slotted between the host's background and its
      // content it would also be sound, but no sample needs it and rebuilding the gradient
      // list for grounded content risks double-painting pseudo stand-ins. The wash child is
      // positioned against the element, so the element has to be its own containing block; on
      // a static box the inset would resolve against some distant ancestor.
      if (computed.position === 'static') continue;
      if (el.children.length > 0 || (el.textContent || '').trim() !== '') continue;
      const urlIndex = layers.findIndex((layer) => /^url\(/i.test(layer));
      if (urlIndex < 0 || (bg.match(/url\(/gi) || []).length !== 1) continue;
      const wash = layers.filter((layer, index) => index !== urlIndex);
      if (wash.length === 0) continue;
      // Each fitting property is a per-layer list; the element now carries a single layer, so it
      // needs that layer's own entry rather than the list. A one-entry value is already shared by
      // every layer and passes through unchanged.
      const entryFor = (value, fallback) => {
        const entries = splitTopLevelCommas(value);
        const picked = entries.length > 1 ? entries[urlIndex] : entries[0];
        return picked || fallback;
      };
      const size = entryFor(computed.getPropertyValue('background-size'), 'auto');
      const position = entryFor(computed.getPropertyValue('background-position'), '0% 0%');
      const repeat = entryFor(computed.getPropertyValue('background-repeat'), 'repeat');
      el.style.backgroundImage = layers[urlIndex];
      el.style.backgroundSize = size;
      el.style.backgroundPosition = position;
      el.style.backgroundRepeat = repeat;
      const tint = document.createElement('div');
      tint.setAttribute('data-pagx-tint', '');
      tint.setAttribute(
        'style',
        'position:absolute;left:0;top:0;width:100%;height:100%;background-image:' +
          wash.join(', ') +
          ';'
      );
      el.appendChild(tint);
    }
  }
  splitTintedBackgrounds();
}

// ===== Pre-snapshot pass: snapshot live <canvas> bitmaps =====

// Walk every <canvas> in the document and stash a `data:image/png;base64,...`
// URI of its current bitmap on `data-snapshot-canvas-src`. The snapshot
// renderer treats a canvas with that attribute as if it were an <img>; charts
// drawn at runtime by ECharts, Chart.js, D3-canvas, etc. therefore survive
// the trip to PAGX as ordinary static images, even though PAGX has no canvas
// model.
//
// Failure modes left as silent drops (snapshot walker treats canvases without
// the attribute as if they were still in DROP_TAGS):
//   - SecurityError ("tainted canvas"): the page drew a cross-origin image
//     into the canvas without CORS, so the browser refuses to serialise it.
//   - WebGL contexts created without `preserveDrawingBuffer: true` may have
//     an empty back buffer by the time we read it. Workaround: pages can
//     preserve the buffer themselves, or call `drawImage` into a 2D canvas
//     before snapshot.
//   - Zero-sized canvas (`width=0` or `height=0`): toDataURL returns the
//     trivial `"data:,"`, which we skip so the renderer drops the element.
async function inlineCanvases() {
  const canvases = Array.from(document.querySelectorAll('canvas'));
  for (const canvas of canvases) {
    if (!canvas.width || !canvas.height) continue;
    let dataUri = '';
    try {
      dataUri = canvas.toDataURL('image/png');
    } catch (err) {
      console.warn(`html-snapshot: canvas.toDataURL failed: ${err && err.message}`);
      continue;
    }
    if (!dataUri || dataUri === 'data:,') continue;
    canvas.setAttribute('data-snapshot-canvas-src', dataUri);
  }
}

/* eslint-enable no-undef, no-inner-declarations */

/* eslint-disable no-undef, no-inner-declarations */

// ===== Pre-snapshot pass: inline remote mask-image urls =====

// Unlike `background-image`, whose remote layers `inlineExternalImages` has already replaced with
// `data:` URIs, a `mask-image: url(https://…)` reaches the walker verbatim. A browser resolves
// such a url over the network, so the subset still renders correctly when opened directly, but the
// importer hands the url to `ImageCodec::MakeFrom`, which cannot fetch: the mask layer is dropped
// and the masked element paints its unmasked fill instead. Fetch every remote mask url and rewrite
// it to a `data:` URI so both consumers see the same self-contained value.
//
// Must run after `materializeDecorativePseudoElements`: a decorative `::before` / `::after` icon
// lives on a synthetic stand-in div that pass creates, and the mask it copies from the pseudo's
// computed style is what this pass has to resolve.
//
// Gradients (the importer rebuilds them into a contour mask), `data:` urls (already
// self-contained) and `file:` urls (`normalizeMaskImage` strips those to a plain path the importer
// loads directly) are left untouched.
async function inlineMaskImages() {
  // Standalone on purpose: `page.evaluate` serialises this function into the page, so it cannot
  // share a closure with `inlineExternalImages`' own helpers.
  function blobToDataUri(blob) {
    return new Promise((resolve, reject) => {
      const reader = new FileReader();
      reader.onload = () => resolve(String(reader.result || ''));
      reader.onerror = () => reject(reader.error || new Error('FileReader failed'));
      reader.readAsDataURL(blob);
    });
  }

  // One entry per element: a decorative pseudo that materialised onto its host, a plain element
  // masked by an icon, and a clamped-text fade overlay all take the same path.
  const pending = [];
  for (const el of Array.from(document.querySelectorAll('*'))) {
    const value = (getComputedStyle(el).getPropertyValue('mask-image') || '').trim();
    if (!value || value === 'none') continue;
    const urls = Array.from(value.matchAll(/url\(\s*(['"]?)([^'")]+)\1\s*\)/gi))
      .map((match) => match[2]).filter((url) => /^https?:/i.test(url));
    if (urls.length) pending.push({ el, urls, value });
  }
  if (pending.length === 0) return;

  // Icons repeat across a page (one chevron asset on every nav item), so the same url is fetched
  // and encoded once.
  const cache = new Map();
  async function resolveUrl(url) {
    if (cache.has(url)) return cache.get(url);
    const controller = new AbortController();
    let timer;
    const timeout = new Promise((_, reject) => {
      timer = setTimeout(() => {
        controller.abort();
        reject(new Error('mask fetch timed out'));
      }, 10000);
    });
    const download = (async () => {
      const res = await fetch(url, { credentials: 'omit', signal: controller.signal });
      if (!res.ok) throw new Error(`fetch ${res.status}`);
      return await blobToDataUri(await res.blob());
    })();
    const promise = Promise.race([download, timeout]).finally(() => clearTimeout(timer));
    cache.set(url, promise);
    return promise;
  }

  // The data URI is emitted single-quoted: the value lands inside the double-quoted `style="…"`
  // attribute the walker writes, where a double-quoted url() would close the attribute early.
  async function inlineOne(entry) {
    try {
      const resolved = new Map();
      for (const url of entry.urls) resolved.set(url, await resolveUrl(url));
      entry.el.style.maskImage = entry.value.replace(
        /url\(\s*(['"]?)([^'")]+)\1\s*\)/gi,
        (original, quote, url) => resolved.has(url) ? `url('${resolved.get(url)}')` : original,
      );
    } catch (err) {
      console.warn(`html-snapshot: failed to inline mask ${entry.urls.join(', ')}: ${err && err.message}`);
    }
  }

  const FETCH_CHUNK_SIZE = 8;
  for (let i = 0; i < pending.length; i += FETCH_CHUNK_SIZE) {
    const slice = pending.slice(i, i + FETCH_CHUNK_SIZE);
    await Promise.all(slice.map(inlineOne));
  }
}

export { inlineExternalImages, inlineCanvases, inlineMaskImages };
