// Copyright (C) 2026 Tencent. All rights reserved.
// @ts-nocheck

/* eslint-enable no-undef, no-inner-declarations */

/* eslint-disable no-undef, no-inner-declarations */

// ===== Pre-snapshot pass: rasterize vector <pattern> tiles =====
//
// PAGX can only express a `<pattern>` fill as an `ImagePattern` (a tiled bitmap). A pattern whose
// content is vector geometry (a stroked wave, a dotted hatch, ...) has no image to tile, so the
// importer drops it and the painted shape loses its texture. The importer cannot rasterize the
// tile itself — the translation stage has no render context — but the browser can: each vector
// pattern's tile is drawn to a canvas here and its content replaced with a single `<image>`
// carrying the tile PNG, which the importer's existing image-pattern path then tiles as usual.
//
// Only `patternUnits="userSpaceOnUse"` tiles with an explicit pixel size are handled: an
// `objectBoundingBox` tile's pixel size depends on the referencing shape's bounds, which this
// pass does not have. Those patterns are left untouched (and dropped, exactly as before).
async function inlineSvgVectorPatterns() {
  const SVG_NS = 'http://www.w3.org/2000/svg';
  const svgs = Array.from(document.querySelectorAll('svg'));
  if (svgs.length === 0) return;

  const loadImage = (src) =>
    new Promise((resolve, reject) => {
      const img = new Image();
      img.onload = () => resolve(img);
      img.onerror = () => reject(new Error('tile image load failed'));
      img.src = src;
    });

  // A pattern whose content is already an `<image>` needs no rasterization — the importer handles
  // it directly and re-encoding would only lose resolution.
  const isImageOnly = (pattern) => {
    let sawAny = false;
    for (const child of Array.from(pattern.children)) {
      const name = child.tagName.toLowerCase();
      if (name === 'title' || name === 'desc') continue;
      sawAny = true;
      if (name !== 'image') return false;
    }
    return sawAny;
  };

  const pending = [];
  for (const svg of svgs) {
    // Only patterns that resolve inside their own SVG are considered; a pattern inherited from
    // another SVG would not resolve once this SVG is serialised standalone either.
    const defs = svg.querySelector('defs');
    for (const pattern of Array.from(svg.querySelectorAll('pattern'))) {
      if ((pattern.getAttribute('patternUnits') || 'objectBoundingBox').toLowerCase() !==
          'userspaceonuse') {
        continue;
      }
      const contentUnits =
        (pattern.getAttribute('patternContentUnits') || 'userSpaceOnUse').toLowerCase();
      if (contentUnits !== 'userspaceonuse') continue;
      const w = parseFloat(pattern.getAttribute('width') || '');
      const h = parseFloat(pattern.getAttribute('height') || '');
      if (!(w > 0) || !(h > 0)) continue;
      if (isImageOnly(pattern)) continue;
      pending.push({
        pattern,
        defs,
        w,
        h,
        x: parseFloat(pattern.getAttribute('x') || '0') || 0,
        y: parseFloat(pattern.getAttribute('y') || '0') || 0,
      });
    }
  }
  if (pending.length === 0) return;

  async function rasterizeOne(entry) {
    const { pattern, defs, w, h, x, y } = entry;
    try {
      const inner = Array.from(pattern.children).map((c) => c.outerHTML).join('');
      const defsMarkup = defs ? defs.outerHTML : '';
      const markup =
        '<svg xmlns="' + SVG_NS + '" width="' + w + '" height="' + h + '" viewBox="' + x + ' ' + y +
        ' ' + w + ' ' + h + '">' + defsMarkup + inner + '</svg>';
      const dataUri =
        'data:image/svg+xml;base64,' + btoa(unescape(encodeURIComponent(markup)));
      const img = await loadImage(dataUri);
      const canvas = document.createElement('canvas');
      canvas.width = Math.max(1, Math.round(w));
      canvas.height = Math.max(1, Math.round(h));
      const ctx = canvas.getContext('2d');
      ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
      const png = canvas.toDataURL('image/png');
      while (pattern.firstChild) pattern.removeChild(pattern.firstChild);
      const image = document.createElementNS(SVG_NS, 'image');
      image.setAttribute('href', png);
      image.setAttribute('x', String(x));
      image.setAttribute('y', String(y));
      image.setAttribute('width', String(w));
      image.setAttribute('height', String(h));
      pattern.appendChild(image);
    } catch (err) {
      console.warn(
        'html-snapshot: failed to rasterize vector <pattern>: ' + (err && err.message),
      );
    }
  }

  // Decoding the tile image is async; rasterize a few at a time to bound the work.
  const CHUNK = 8;
  for (let i = 0; i < pending.length; i += CHUNK) {
    await Promise.all(pending.slice(i, i + CHUNK).map(rasterizeOne));
  }
}

export { inlineSvgVectorPatterns };
