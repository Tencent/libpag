// Copyright (C) 2026 Tencent. All rights reserved.
// @ts-nocheck

// Write `value` as a double-quoted CSS string that also survives the importer's
// XML parse of the `<style>` text. `<`, `>` and `&` would be read as markup or
// entities there, so they are written as CSS hex escapes like the quote,
// backslash and control characters; the browser and the importer's CSS reader
// both decode them back to the original character.
function cssQuotedString(value) {
  let out = '"';
  for (const ch of String(value)) {
    if (ch === '"' || ch === '\\' || ch === '<' || ch === '>' || ch === '&' || ch < ' ') {
      out += '\\' + ch.codePointAt(0).toString(16) + ' ';
    } else {
      out += ch;
    }
  }
  return out + '"';
}

// ===== Face descriptors =====
//
// A `@font-face` rule names one cut of a family, and its weight/style descriptors decide
// which cut the browser paints: a family declared only at 700 answers the page's 400 text
// with 700 outlines. The subset and the catalog match built from it need that cut too.
//
// Shared by the browser payloads and the Node-side matcher, so every helper below stays a
// standalone `function` declaration that serializes correctly via `toString()`.

interface FontFaceDescriptor {
  family: string;
  weight: string;
  style: string;
}

// A descriptor pair plus the source the icon-font pass resolved it to.
interface FontFaceEntry extends FontFaceDescriptor {
  url: string;
  format: string;
}

// Canonical weight descriptor: `normal`/`bold` keywords, a single weight, or a variable
// range, with every value inside the CSS 1..1000 range and `lo <= hi`. Anything else is
// invalid as a descriptor, so it falls back to the initial 400 like the importer does.
function normalizeFontFaceWeight(value) {
  const v = String(value || '').trim().toLowerCase();
  if (!v || v === 'normal') return '400';
  if (v === 'bold') return '700';
  const number = '-?(?:\\d+(?:\\.\\d+)?|\\.\\d+)(?:e[-+]?\\d+)?';
  const match = new RegExp('^(' + number + ')(?:\\s+(' + number + '))?$').exec(v);
  if (!match) return '400';
  // Endpoints keep their exact value: `400.9 400.1` is reversed only as floats, and the
  // importer compares the same way, so nothing is rounded before the range check.
  const lo = Number(match[1]);
  const hi = match[2] === undefined ? lo : Number(match[2]);
  if (!Number.isFinite(lo) || !Number.isFinite(hi)) return '400';
  if (lo < 1 || lo > 1000 || hi < 1 || hi > 1000 || lo > hi) return '400';
  return match[2] === undefined ? String(lo) : lo + ' ' + hi;
}

// Canonical slant: `normal`, `italic`, or `oblique` with an optional angle. The angle must
// carry a CSS unit, be finite and land inside -90..90 degrees, which is what Chromium
// accepts in a @font-face rule; anything else leaves the descriptor at its initial normal.
function normalizeFontFaceStyle(value) {
  const v = String(value || '').trim().toLowerCase();
  if (v === 'italic') return 'italic';
  const number = '-?(?:\\d+(?:\\.\\d+)?|\\.\\d+)(?:e[-+]?\\d+)?';
  const oblique = new RegExp('^oblique(?:\\s+(' + number + ')(deg|grad|rad|turn))?$').exec(v);
  if (!oblique) return 'normal';
  if (oblique[1] === undefined) return 'oblique';
  const angle = Number(oblique[1]);
  if (!Number.isFinite(angle)) return 'normal';
  const degrees = oblique[2] === 'grad' ? angle * 0.9
    : oblique[2] === 'rad' ? angle * 180 / Math.PI
      : oblique[2] === 'turn' ? angle * 360 : angle;
  return degrees >= -90 && degrees <= 90 ? 'oblique' : 'normal';
}

// The weight range a face can render: a single value for a static cut, `lo hi` for a
// variable one. The descriptor is already validated, so the bounds are ordered.
function fontFaceWeightRange(face) {
  const nums = normalizeFontFaceWeight(face && face.weight).match(/\d+(?:\.\d+)?/g) || [];
  const lo = nums.length > 0 ? Number(nums[0]) : 400;
  return [lo, nums.length > 1 ? Number(nums[1]) : lo];
}

// Return the CSS Fonts weight-search phase and distance for a face range. The
// phase captures the spec's directional preference: below 400 search downward
// first, from 400 through 500 search upward to 500 then downward, and above 500
// search upward first. This also makes equidistant choices independent of
// @font-face declaration order (for example 650 chooses 900 over 400).
function fontWeightPreference(requested, lo, hi) {
  if (requested >= lo && requested <= hi) return [0, 0];
  if (requested < 400) {
    return hi < requested ? [1, requested - hi] : [2, lo - requested];
  }
  if (requested > 500) {
    return lo > requested ? [1, lo - requested] : [2, requested - hi];
  }
  if (lo > requested && lo <= 500) return [1, lo - requested];
  if (hi < requested) return [2, requested - hi];
  return [3, lo - requested];
}

// Pick the @font-face that best matches a requested weight and slant.
// A family may expose several physical files under one name (Font Awesome's
// "Font Awesome 6 Free" uses Regular/400 and Solid/900). Treating a family as
// a single URL silently chooses the first declaration and makes every glyph
// that exists only in another weight disappear.
function selectFontFace(faces, requestedWeight, requestedStyle) {
  if (!faces || faces.length === 0) return null;
  const reqWeightText = normalizeFontFaceWeight(requestedWeight);
  const reqWeightMatch = reqWeightText.match(/\d+(?:\.\d+)?/);
  const reqWeight = reqWeightMatch ? Number(reqWeightMatch[0]) : 400;
  const reqStyle = normalizeFontFaceStyle(requestedStyle);
  let best = null;
  let bestPreference = null;
  for (const face of faces) {
    if (!face) continue;
    const [lo, hi] = fontFaceWeightRange(face);
    const [weightPhase, weightDistance] = fontWeightPreference(reqWeight, lo, hi);
    // Style should dominate weight preference: an italic face at the exact
    // weight is a worse match than a nearby upright face for normal text.
    const preference = [
      normalizeFontFaceStyle(face.style) === reqStyle ? 0 : 1,
      weightPhase,
      weightDistance,
    ];
    if (
      bestPreference === null ||
      preference[0] < bestPreference[0] ||
      (preference[0] === bestPreference[0] && preference[1] < bestPreference[1]) ||
      (preference[0] === bestPreference[0] && preference[1] === bestPreference[1] &&
        preference[2] < bestPreference[2])
    ) {
      best = face;
      bestPreference = preference;
    }
  }
  return best;
}

// The weight to match on once `face` won. A request is only ever promoted to a heavier
// declared cut, never lowered: a 700 heading served by a lone 400 face keeps its 700
// (the browser synthesises the bold), while a 400 request answered by a 700 face takes
// 700 because those are the outlines the page actually painted.
function fontFaceRenderedWeight(face, requestedWeight) {
  const lo = fontFaceWeightRange(face)[0];
  const weight = Number(requestedWeight);
  if (!Number.isFinite(weight)) return lo;
  return weight > lo ? weight : lo;
}

// The slant the browser paints with: a slanted face wins even for an upright request,
// while an upright face leaves the requested slant in place so the catalog still resolves
// to a slanted face, matching the previous behaviour.
function fontFaceRenderedStyle(face, requestedStyle) {
  const faceStyle = normalizeFontFaceStyle(face && face.style);
  return faceStyle === 'normal' ? requestedStyle : faceStyle;
}

// Serialise one carried-over rule. The descriptors travel with the source so the
// subset, a re-opened snapshot and the catalog match all see the same cut.
function fontFaceRuleCss(face) {
  return '@font-face{font-family:' + cssQuotedString(face.family) +
    ';src:url(' + cssQuotedString(face.src) + ')' +
    ';font-weight:' + normalizeFontFaceWeight(face.weight) +
    ';font-style:' + normalizeFontFaceStyle(face.style) + '}';
}

// Collect the page's `@font-face` rules the subset can carry over, as
// `{ family, src, weight, style }` records in stylesheet order. `src` is either
// the absolute path of a font file on local disk or the rule's original
// `data:` URI. The importer registers either form under `family` — a file
// becomes a PAGX `<Font file>` resource, a `data:` URI is decoded into the
// document's font config — so the CSS name the subset carries always resolves,
// however it relates to the family name stored inside the file. Remote sources
// are left out: the page fetch that produced them no longer exists once the
// subset is handed to the importer.
// Each rule contributes its first `url()` source, ignoring `local()` names and
// `format()` hints. Rules nested in grouping rules (`@media`, `@supports`) and
// imported sheets are visited too. A sheet whose rules are inaccessible
// (cross-origin) is skipped, which loses nothing: its fonts cannot be local.
// Faces are de-duplicated by family, source and descriptors together, so the
// several cuts one family declares survive as separate rules.
function collectCarryOverFontFaces(sheets, baseURI) {
  const faces = [];
  const seen = new Set();
  const URL_RE = /url\(\s*(?:"((?:[^"\\]|\\.)*)"|'((?:[^'\\]|\\.)*)'|([^)\s]*))\s*\)/gi;
  const unescape = (text) => text.replace(/\\(.)/g, '$1');
  // A `data:` URI is already self-contained, so it travels verbatim; anything else has to
  // resolve to a file on local disk.
  const toCarryOverSource = (raw, base) => {
    if (/^data:/i.test(raw)) {
      return raw;
    }
    try {
      const u = new URL(raw, base);
      if (u.protocol !== 'file:') return '';
      let p = decodeURIComponent(u.pathname);
      if (/^\/[A-Za-z]:/.test(p)) p = p.slice(1);
      return p;
    } catch (_) {
      return '';
    }
  };
  const visitRules = (rules, base, depth) => {
    if (!rules || depth > 8) return;
    for (const rule of Array.from(rules)) {
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      const r = rule as any;
      if (r.styleSheet) {
        visitSheet(r.styleSheet, depth + 1);
        continue;
      }
      if (r.type !== 5) {
        if (r.cssRules) visitRules(r.cssRules, base, depth + 1);
        continue;
      }
      const family = unescape((r.style.getPropertyValue('font-family') || '').trim()
        .replace(/^(["'])([\s\S]*)\1$/, '$2'));
      const src = r.style.getPropertyValue('src') || '';
      if (!family || !src) continue;
      const weight = normalizeFontFaceWeight(r.style.getPropertyValue('font-weight') || '');
      const style = normalizeFontFaceStyle(r.style.getPropertyValue('font-style') || '');
      URL_RE.lastIndex = 0;
      let m;
      while ((m = URL_RE.exec(src)) !== null) {
        const raw = unescape(m[1] !== undefined ? m[1] : (m[2] !== undefined ? m[2] : m[3]));
        const carried = raw ? toCarryOverSource(raw, base) : '';
        if (!carried) continue;
        const key = family + '\n' + carried + '\n' + weight + '\n' + style;
        if (!seen.has(key)) {
          seen.add(key);
          faces.push({ family, src: carried, weight, style });
        }
        break;
      }
    }
  };
  const visitSheet = (sheet, depth) => {
    let rules;
    try { rules = sheet.cssRules; } catch (_) { return; }
    visitRules(rules, sheet.href || baseURI, depth);
  };
  for (const sheet of Array.from(sheets || [])) {
    visitSheet(sheet, 0);
  }
  return faces;
}

// ===== Payload sharing =====
//
// `Function.prototype.toString()` keeps a function body verbatim, so a serialized helper
// can only call names that exist in the payload scope. An `import` compiles to a module
// reference (`module_1.name`) that does not exist there, so the other payloads (the
// icon-font pass) reach these helpers through this table, which is emitted into their
// scope from `FONT_FACE_HELPERS_SRC`.
function fontFaceHelpers() {
  return {
    fontFaceWeightRange,
    fontWeightPreference,
    normalizeFontFaceStyle,
    normalizeFontFaceWeight,
    selectFontFace,
  };
}
const FONT_FACE_HELPER_FNS = [
  fontFaceWeightRange,
  fontWeightPreference,
  normalizeFontFaceStyle,
  normalizeFontFaceWeight,
  selectFontFace,
];
const FONT_FACE_HELPERS_SRC = FONT_FACE_HELPER_FNS.map((fn) => fn.toString()).join('\n\n') +
  '\n\nconst FONT_FACE_HELPERS = { ' + Object.keys(fontFaceHelpers()).join(', ') + ' };';

export {
  collectCarryOverFontFaces,
  cssQuotedString,
  fontFaceRenderedStyle,
  fontFaceRenderedWeight,
  fontFaceRuleCss,
  fontFaceWeightRange,
  fontWeightPreference,
  normalizeFontFaceStyle,
  normalizeFontFaceWeight,
  selectFontFace,
  fontFaceHelpers,
  FONT_FACE_HELPERS_SRC,
};
export type { FontFaceDescriptor, FontFaceEntry };
