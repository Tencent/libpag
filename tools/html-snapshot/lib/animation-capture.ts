// Animation capture → canonical `@keyframes` + `animation`.
//
// The PAGX HTML importer plays back animations declared as CSS `@keyframes` +
// the `animation` shorthand (see spec/html_subset.md §13). Real pages produce
// animation from many sources, though: plain CSS `@keyframes`, the Web
// Animations API (WAAPI), and JS libraries (GSAP, anime.js, Motion One, …)
// that mutate styles imperatively. This module normalises all of them into the
// single declarative form the importer understands.
//
// Strategy (priority chain, all running in the page before `takeSnapshot()`):
//   1. WAAPI — `document.getAnimations()` + `effect.getKeyframes()` /
//      `getComputedTiming()`. This is the unifying bridge: CSS animations,
//      Motion One and web-animations-js all surface here.
//   2. CSS `@keyframes` — fallback for engines/cases where `getAnimations()`
//      is unavailable: read the matching `CSSKeyframesRule` for each element
//      whose computed `animation-name` is set.
//   3. rAF libraries (GSAP, anime.js) — deterministic seeking: pause the
//      library clock, seek to N sample times, read `getComputedStyle`, and
//      reduce the samples to compact keyframes.
//
// Every captured animation is rewritten into one canonical form:
//   - a `@keyframes pagxAnim<N>` rule injected into a dedicated
//     `<style id="__pagx_anim_keyframes">` block in `<head>`, restricted to
//     the channels the runtime can play (`opacity`, `transform: translate`,
//     `color`, `background-color`);
//   - an inline `animation: pagxAnim<N> <dur> <timing> <delay> <iter> <dir>`
//     on the element.
//
// `browser-snapshot.ts` then carries the keyframes block into the output
// `<style>` (snapshotMain) and emits the `animation-*` longhands per element
// (buildStyle → appendAnimation), so the importer sees the canonical subset.
//
// Structure: the pure, DOM-independent helpers are module-scope exports so they
// can be unit-tested in Node (test/animation-capture.test.js). The DOM
// collectors and the orchestrator are also module-scope so the whole set can be
// serialised into one in-browser IIFE (mirroring browser-snapshot.ts's
// HELPER_FNS pattern); `capturePagxAnimationsOnPage` evaluates that payload in
// the page.

// @ts-nocheck

import { errMessage } from './common';
import type { Page } from './browser-engine';

/* eslint-disable no-undef */

export const PAGX_ANIM_PREFIX = 'pagxAnim';
export const PAGX_ANIM_STYLE_ID = '__pagx_anim_keyframes';

// Init-script injected before any page script runs, keeping animations
// *suspended* across the settle wait so they can be measured and seeked later.
//   1. A `<style>` rule pinning `animation-play-state: paused` on every element
//      so finite CSS animations do not tick through the settle, finish, drop
//      into `idle`, and disappear from `document.getAnimations()`.
//   2. An `Element.prototype.animate` wrapper that pauses every WAAPI animation
//      on creation (CSS play-state does not apply to WAAPI animations).
// Both must be installed before any page script runs, so callers ship this via
// page-loader's `initScripts`. The capture pipeline (lib/snapshot-runner.ts)
// and the baseline (eval-animation/baseline-frames.js) install the identical
// script so both observe the same suspended animations; the seek path drives
// playback explicitly via `currentTime` / GSAP `time()` / anime `seek()`.
export const PAGX_ANIM_PAUSE_INIT_SCRIPT = `(function() {
  var STYLE_ID = '__pagxBaselinePauseAnims';
  var STYLE_TEXT = '*, *::before, *::after { animation-play-state: paused !important; }';
  function inject() {
    if (document.getElementById(STYLE_ID)) return;
    var parent = document.head || document.documentElement;
    if (!parent) return;
    var s = document.createElement('style');
    s.id = STYLE_ID;
    s.textContent = STYLE_TEXT;
    parent.appendChild(s);
  }
  inject();
  if (!document.getElementById(STYLE_ID)) {
    document.addEventListener('DOMContentLoaded', inject, { once: true });
  }
  try {
    var proto = window.Element && Element.prototype;
    var orig = proto && proto.animate;
    if (typeof orig === 'function') {
      proto.animate = function() {
        var anim = orig.apply(this, arguments);
        try { if (anim && typeof anim.pause === 'function') anim.pause(); } catch (_) {}
        return anim;
      };
    }
  } catch (_) {}
})();`;

// Init-script installing a *virtual clock* that freezes the page's JS timers
// (`setTimeout` / `setInterval` / `requestAnimationFrame`) and time sources
// (`Date.now` / `performance.now`) so they only advance when driven explicitly
// via `window.__pagxClock.advanceTo(ms)`.
//
// Why: many "animated" pages are not declarative at all — they run a
// setTimeout/interval state machine that toggles classes (or calls
// `element.click()`) to swap whole scenes over time (e.g. a menu that slides
// out and a skill panel that fades in). `document.getAnimations()` sees none of
// that at snapshot time, and left on the wall clock the mutations fire
// nondeterministically during load/settle. By intercepting the timers at
// document-start and advancing them on the SAME absolute clock the animation
// sampler and the eval baseline already seek declarative animations to
// (`pagxSeekAllToTime` → `advanceTo`), the JS-driven scene changes become part
// of the sampled timeline: the sampler observes each element's opacity /
// transform / color as the scene switches, and encodes it as canonical
// `@keyframes` the importer can replay.
//
// Must be installed before any page script runs (page-loader `initScripts`),
// alongside PAGX_ANIM_PAUSE_INIT_SCRIPT. `advanceTo` is forward-only (fired
// timers cannot be un-fired); callers seek monotonically. Frame ticks for rAF
// are only generated while rAF callbacks are pending, so a page that uses no
// rAF costs nothing. Exceptions thrown by page callbacks are swallowed so one
// bad handler cannot abort the whole advance.
export const PAGX_VIRTUAL_CLOCK_INIT_SCRIPT = `(function(){
  if (window.__pagxClock) return;
  var FRAME = 1000 / 60;
  var EPOCH = 1700000000000;
  var GUARD = 5000000;
  var virtualNow = 0;
  var lastFrame = 0;
  var seq = 1;
  var timers = [];
  var rafs = [];

  function schedule(fn, delay, args, repeat){
    if (typeof fn !== 'function') {
      var code = String(fn);
      fn = function(){ try { (0, eval)(code); } catch(_){} };
    }
    var d = +delay; if (!isFinite(d) || d < 0) d = 0;
    var id = seq++;
    timers.push({ id: id, time: virtualNow + d, fn: fn, args: args || [],
                  interval: repeat ? Math.max(1, d) : null, cancelled: false });
    return id;
  }
  window.setTimeout = function(fn, delay){
    return schedule(fn, delay, Array.prototype.slice.call(arguments, 2), false);
  };
  window.setInterval = function(fn, delay){
    return schedule(fn, delay, Array.prototype.slice.call(arguments, 2), true);
  };
  window.clearTimeout = function(id){
    for (var i = 0; i < timers.length; i++){ if (timers[i].id === id){ timers[i].cancelled = true; return; } }
  };
  window.clearInterval = window.clearTimeout;
  window.requestAnimationFrame = function(cb){ var id = seq++; rafs.push({ id: id, cb: cb, cancelled: false }); return id; };
  window.cancelAnimationFrame = function(id){
    for (var i = 0; i < rafs.length; i++){ if (rafs[i].id === id){ rafs[i].cancelled = true; return; } }
  };
  try { window.Date.now = function(){ return EPOCH + Math.round(virtualNow); }; } catch(_){}
  try { if (window.performance) window.performance.now = function(){ return virtualNow; }; } catch(_){}

  function earliestTimer(){
    var m = Infinity;
    for (var i = 0; i < timers.length; i++){ var t = timers[i]; if (!t.cancelled && t.time < m) m = t.time; }
    return m;
  }
  // Tag every animation that is live *now* with its birth time (the virtual
  // instant it first appeared) unless already tagged. CSS animations created by
  // a timer-driven class toggle / scripted click get birth = the firing timer's
  // virtual time; anything present before the first timer fires stays untagged
  // and is treated as born at 0 by the seek. This runs on the deterministic
  // virtual clock, so the same births are recorded regardless of how densely a
  // caller samples — keeping the baseline and the capture on one clock.
  function tagBirths(birth){
    try {
      if (typeof document.getAnimations !== 'function') return;
      var live = document.getAnimations();
      for (var i = 0; i < live.length; i++){
        var a = live[i];
        try { if (a.__pagxBirth === undefined) a.__pagxBirth = birth; } catch(_){}
      }
    } catch(_){}
  }
  function fireDue(now){
    var guard = 0;
    while (guard++ < GUARD){
      var best = null;
      for (var i = 0; i < timers.length; i++){
        var t = timers[i];
        if (t.cancelled || t.time > now) continue;
        if (best === null || t.time < best.time || (t.time === best.time && t.id < best.id)) best = t;
      }
      if (best === null) break;
      var firedAt = best.time;
      if (best.interval != null) best.time += best.interval; else best.cancelled = true;
      try { best.fn.apply(window, best.args); } catch(_){}
      // Any animation the callback just created (e.g. a click-class toggling on
      // a 0.42s pulse) is born at this timer's virtual time.
      tagBirths(firedAt);
    }
    if (timers.length > 4096){
      var kept = [];
      for (var j = 0; j < timers.length; j++){ if (!timers[j].cancelled) kept.push(timers[j]); }
      timers = kept;
    }
  }
  function flushRaf(ts){
    var due = rafs; rafs = [];
    for (var i = 0; i < due.length; i++){ if (due[i].cancelled) continue; try { due[i].cb(ts); } catch(_){} }
  }
  function advanceTo(target){
    target = +target;
    if (!isFinite(target)) return;
    if (target < virtualNow) target = virtualNow;
    var guard = 0;
    while (guard++ < GUARD){
      var nt = earliestTimer();
      var nf = rafs.length ? (lastFrame + FRAME) : Infinity;
      var next = nt < nf ? nt : nf;
      if (!(next <= target)) break;
      if (next < virtualNow) next = virtualNow;
      virtualNow = next;
      if (nf <= nt && rafs.length && next === nf){ lastFrame = nf; flushRaf(virtualNow); }
      fireDue(virtualNow);
    }
    if (target > virtualNow) virtualNow = target;
    if (rafs.length === 0) lastFrame = virtualNow;
  }

  window.__pagxClock = { now: function(){ return virtualNow; }, advanceTo: advanceTo };
})();`;

export interface CaptureAnimationsOptions {
  // Number of samples taken across a library timeline (GSAP / anime.js) before
  // per-channel RDP decimation. Higher is denser/more faithful; RDP keeps the
  // emitted keyframe count proportional to curve complexity, not this value.
  sampleCount?: number;
  // Upper bound on elements probed during library sampling (keeps pathological
  // pages from blowing up the O(samples × elements) probe).
  maxElements?: number;
  logger?: (msg: string) => void;
}

export interface CaptureAnimationsResult {
  count: number;
  names: string[];
}

// A normalised keyframe stop: a 0..1 offset and the subset property bag.
export interface PagxAnimStop {
  offset: number;
  props: Record<string, string>;
}

// A captured animation without its DOM element, i.e. everything needed to emit
// the canonical `@keyframes` + `animation` shorthand. `pagxAnimMain` adds the
// `el` reference on top of this when collecting in the page.
export interface PagxAnimDescriptor {
  keyframes: PagxAnimStop[];
  durationMs: number;
  delayMs: number;
  iterations: number;
  direction: string;
  timing: string;
}

// One distinct text value a dynamic-text element shows during the timeline,
// together with the styling it carried while visible and the opacity curve to
// fade it in/out. `pagxBuildTextOverlays` materialises each segment as a
// separate overlay text layer so the imported PAGX can show the changing
// glyph (e.g. a combo counter's `+1 … +20`) that a single static text node
// could never reproduce.
export interface PagxTextSegment {
  text: string;
  fontSize: string | null;
  // Text paint captured at the segment's most-visible sample. `color` drives
  // HTML text; `fill` drives SVG `<text>` (whose visible paint is `fill`, not
  // `color`) — both are pinned on the overlay so either element kind renders in
  // the right hue. Cloning drops the source `id`, which would otherwise break an
  // author `#id` fill rule, so the sampled paint is the only reliable source.
  color: string | null;
  fill: string | null;
  // Opacity keyframes over the *whole* global window (offsets 0..1): the
  // element's sampled opacity inside this segment's window, 0 everywhere else.
  stops: PagxAnimStop[];
}

// A text-leaf element whose `textContent` changed across the sampled timeline,
// with one segment per contiguous run of a distinct non-empty value.
export interface PagxTextDynamic {
  el: HTMLElement;
  segments: PagxTextSegment[];
}

// ===== Pure helpers (module scope, exported, unit-tested) =====

// transform → translate-only normalisation. The runtime can only play
// translation on a Layer (no rotation/scale/skew channel), so any
// non-translation transform component is dropped. The *translation* part is
// still kept, however: a `matrix(...)` produced by e.g. GSAP that combines a
// translate with a scale/rotation (the common case — `getComputedStyle`
// always reports `matrix(...)`, never the authored `translate() scale()`)
// still carries its net translation in the e/f components, and the importer's
// own transform handling deliberately keeps that translate while dropping
// scale/rotation. Returns null only when there is no translation to play.
//
// `box` (optional): the element's used box size. When provided, percentage
// translations (`translate(-50%, 0)` — common for marquee idioms) are resolved
// to absolute pixels per the CSS spec (x relative to box.width, y relative to
// box.height). The PAGX importer rejects percent values in @keyframes
// (`ParseTransformMatrix` requires absolute lengths and would drop the entire
// keyframe transform with a diagnostic), so resolving here is the only way the
// motion survives. When `box` is omitted (e.g. unit-test contexts), percent
// tokens are forwarded verbatim — preserving the previous behaviour and
// surfacing the importer's diagnostic if such a value is ever fed in.
export function pagxExtractTranslate(
  transformStr: string,
  box?: { width: number; height: number } | null,
): string | null {
  const s = (transformStr || '').trim().toLowerCase();
  if (!s || s === 'none') return null;
  let m = s.match(/^matrix\(([^)]+)\)$/);
  if (m) {
    const n = m[1].split(',').map((x) => parseFloat(x.trim()));
    // Affine 2D matrix: a,b,c,d carry scale/rotation/skew (dropped — no runtime
    // channel), e,f carry the translation we keep.
    if (n.length === 6 && n.every((v) => !isNaN(v))) {
      return 'translate(' + n[4] + 'px, ' + n[5] + 'px)';
    }
    return null;
  }
  m = s.match(/^matrix3d\(([^)]+)\)$/);
  if (m) {
    const n = m[1].split(',').map((x) => parseFloat(x.trim()));
    // 4×4 column-major matrix: indices 12,13 are the x/y translation.
    if (n.length === 16 && n.every((v) => !isNaN(v))) {
      return 'translate(' + n[12] + 'px, ' + n[13] + 'px)';
    }
    return null;
  }
  // Resolve a single translate axis token (e.g. "-50%", "12px", "8") to a CSS
  // length string. `basisPx` is the box dimension to resolve percentages
  // against (width for x, height for y); when null we cannot resolve the
  // percentage, so the original token is preserved verbatim.
  function resolveAxis(token: string | null, basisPx: number | null): string {
    const t = (token || '').trim();
    if (!t) return '0px';
    if (t.endsWith('%')) {
      if (basisPx == null || !isFinite(basisPx)) return t;
      const pct = parseFloat(t);
      if (!isFinite(pct)) return t;
      return (pct / 100 * basisPx) + 'px';
    }
    return t;
  }
  let tx: string | null = null;
  let ty: string | null = null;
  let hadTranslate = false;
  const re = /(translate[xy]?)\(([^)]*)\)/g;
  let mm: RegExpExecArray | null;
  while ((mm = re.exec(s)) !== null) {
    hadTranslate = true;
    const fn = mm[1];
    const args = mm[2].split(',').map((a) => a.trim());
    if (fn === 'translatex') tx = args[0];
    else if (fn === 'translatey') ty = args[0];
    else {
      tx = args[0];
      if (args.length > 1) ty = args[1];
    }
  }
  if (!hadTranslate) return null;
  const wPx = box && isFinite(box.width) ? box.width : null;
  const hPx = box && isFinite(box.height) ? box.height : null;
  return 'translate(' + resolveAxis(tx, wPx) + ', ' + resolveAxis(ty, hPx) + ')';
}

// Like `pagxExtractTranslate`, but preserves the full 2D affine transform when
// its linear part (scale / rotation / skew) is not identity. The runtime CAN
// play a general affine via the importer's `matrix` channel
// (HTMLAnimationBuilder `ParseTransformMatrix` -> matrixKeys), so a click "pop"
// (`transform: scale(1.3)`), a spin (`rotate`), etc. survive capture instead of
// being flattened to their static frame. Pure-translate transforms still
// collapse to `translate(xpx, ypx)` so the common slide / position path keeps
// its `tx`/`ty` decimation and output shape byte-for-byte unchanged.
//
// `getComputedStyle().transform` always resolves to a `matrix(...)` /
// `matrix3d(...)` / `none` form (never the friendly `scale()` / `translate()`
// functions), so the matrix branch is the one the global sampler exercises. The
// friendly compound-string branch only reaches here from the WAAPI / transition
// path; there we still fold a non-identity linear part into a `matrix(...)` via
// a temporary element so scale / rotate authored directly in keyframes is kept
// too, and fall back to translate-only extraction when the value cannot be
// resolved to a matrix (e.g. a 3D transform).
export function pagxExtractTransform(
  transformStr: string,
  box?: { width: number; height: number } | null,
): string | null {
  const s = (transformStr || '').trim().toLowerCase();
  if (!s || s === 'none') return null;
  const m = s.match(/^matrix\(([^)]+)\)$/);
  if (m) {
    const n = m[1].split(',').map((x) => parseFloat(x.trim()));
    if (n.length === 6 && n.every((v) => !isNaN(v))) {
      const linearIsIdentity =
        Math.abs(n[0] - 1) < 1e-6 && Math.abs(n[1]) < 1e-6 &&
        Math.abs(n[2]) < 1e-6 && Math.abs(n[3] - 1) < 1e-6;
      if (linearIsIdentity) return 'translate(' + n[4] + 'px, ' + n[5] + 'px)';
      return 'matrix(' + n.join(', ') + ')';
    }
    return null;
  }
  // matrix3d(...) has no 2D affine equivalent the runtime can play; keep only
  // its translation (mirrors pagxExtractTranslate's matrix3d branch).
  if (/^matrix3d\(/.test(s)) return pagxExtractTranslate(transformStr, box);
  // Friendly compound string (WAAPI / transition keyframe author value): if it
  // is pure translate, reuse the percentage-resolving translate path; otherwise
  // resolve the whole chain to a matrix via a throwaway element so any scale /
  // rotate / skew is preserved.
  if (!/\b(scale|rotate|skew|matrix)/.test(s)) {
    return pagxExtractTranslate(transformStr, box);
  }
  try {
    const probe = document.createElement('div');
    probe.style.position = 'absolute';
    probe.style.transform = transformStr;
    (document.body || document.documentElement).appendChild(probe);
    const resolved = getComputedStyle(probe).transform;
    probe.remove();
    if (resolved && resolved !== 'none') return pagxExtractTransform(resolved, box);
  } catch (_) {
    /* fall through to translate-only */
  }
  return pagxExtractTranslate(transformStr, box);
}

// Resolve any tracked transform value the sampler / emitter produces
// (`translate(xpx, ypx)` or `matrix(a,b,c,d,e,f)`) to its six affine
// components, so decimation can run RDP over a single uniform representation.
// A translate collapses to `[1, 0, 0, 1, tx, ty]`; identical scalar output to
// the old `tx`/`ty` path for pure-translate animations (the constant linear
// channels reduce to their endpoints), plus real scale / rotation curvature for
// matrix keyframes. Returns null when the value is neither form.
export function pagxTransformToMatrix6(value: string): number[] | null {
  const v = (value || '').trim();
  if (!v) return null;
  const mm = v.match(/^matrix\(([^)]+)\)$/i);
  if (mm) {
    const parts = mm[1].split(',').map((x) => parseFloat(x.trim()));
    if (parts.length === 6 && parts.every((x) => !isNaN(x))) return parts;
    return null;
  }
  const t = pagxParseTranslateXY(v);
  if (t) return [1, 0, 0, 1, t[0], t[1]];
  return null;
}

export function pagxPickProp(
  obj: Record<string, unknown>,
  kebab: string,
  camel: string,
): string | null {
  if (obj[kebab] != null && obj[kebab] !== '') return String(obj[kebab]);
  if (obj[camel] != null && obj[camel] !== '') return String(obj[camel]);
  return null;
}

// Reduce a raw property bag (WAAPI keyframe or computed-style sample) to the
// tracked, runtime-playable subset (kebab-cased keys). `box` is forwarded to
// `pagxExtractTranslate` so percentage translations (e.g. `translate(-50%)`)
// resolve to absolute pixels — the importer rejects percent values in
// @keyframes.
export function pagxNormalizeProps(
  raw: Record<string, unknown>,
  box?: { width: number; height: number } | null,
): Record<string, string> {
  const out: Record<string, string> = {};
  const op = pagxPickProp(raw, 'opacity', 'opacity');
  if (op != null) out['opacity'] = op;
  const tr = pagxPickProp(raw, 'transform', 'transform');
  if (tr != null && tr !== 'none') {
    const t = pagxExtractTransform(tr, box);
    if (t) out['transform'] = t;
  }
  const col = pagxPickProp(raw, 'color', 'color');
  if (col != null) out['color'] = col;
  const bg = pagxPickProp(raw, 'background-color', 'backgroundColor');
  if (bg != null) out['background-color'] = bg;
  // `filter` (glow / shadow / blur). Kept verbatim (including `none`) so a
  // none <-> drop-shadow(...) transition is preserved as a varying channel; the
  // importer lowers it onto animatable DropShadowFilter / BlurFilter channels.
  // An animated `box-shadow` glow is folded in as an equivalent drop-shadow so
  // it rides the same channel (the runtime has no box-shadow animation channel).
  const flt = pagxPickProp(raw, 'filter', 'filter');
  const shadowFilter = pagxBoxShadowToFilter(pagxPickProp(raw, 'box-shadow', 'boxShadow'));
  let filterVal = flt;
  if (shadowFilter) {
    filterVal = filterVal != null && filterVal !== 'none'
      ? filterVal + ' ' + shadowFilter
      : shadowFilter;
  }
  if (filterVal != null) out['filter'] = filterVal;
  // SVG paint channels. `fill` / `stroke` are colours (a `<path>` line-draw fills in from
  // `transparent`), `stroke-dashoffset` is a scalar used for the path-trace line-draw idiom
  // (`stroke-dasharray:1; stroke-dashoffset:1 -> 0` with `pathLength="1"`). The importer maps these
  // onto the per-shape Fill / Stroke painter channels; `stroke-dashoffset` is rescaled there by the
  // real path length. Only inline-SVG shapes ever carry a varying value here; HTML elements report
  // a constant default and are dropped by `pagxWhichVary`.
  const fill = pagxPickProp(raw, 'fill', 'fill');
  if (fill != null) out['fill'] = fill;
  const stroke = pagxPickProp(raw, 'stroke', 'stroke');
  if (stroke != null) out['stroke'] = stroke;
  const dashOffset = pagxPickProp(raw, 'stroke-dashoffset', 'strokeDashoffset');
  if (dashOffset != null) out['stroke-dashoffset'] = dashOffset;
  return out;
}

// Assign evenly spaced offsets to any keyframes that lack an explicit one.
export function pagxFillOffsets(norm: Array<{ offset: number | null; props: Record<string, string> }>): void {
  const n = norm.length;
  for (let i = 0; i < n; i++) {
    if (norm[i].offset == null || isNaN(norm[i].offset as number)) {
      norm[i].offset = n === 1 ? 0 : i / (n - 1);
    }
  }
}

export function pagxOffsetFromKeyText(keyText: string): number | null {
  const t = (keyText || '').trim().toLowerCase();
  if (t === 'from') return 0;
  if (t === 'to') return 1;
  if (t.endsWith('%')) {
    const v = parseFloat(t);
    return isNaN(v) ? null : v / 100;
  }
  return null;
}

export function pagxParseTimeMs(v: string): number {
  const t = (v || '').trim().toLowerCase();
  if (!t) return 0;
  if (t.endsWith('ms')) return parseFloat(t) || 0;
  if (t.endsWith('s')) return (parseFloat(t) || 0) * 1000;
  return 0;
}

// Given N computed-style samples for one element, return the subset properties
// whose value changes across the timeline (so static properties are dropped).
// Convert a computed CSS `clip-path` value into a canonical SVG path `d` string in the element's
// border-box pixel space. Used for animated clip-path capture: the importer parses each keyframe's
// `path("d")` back into a point list and drives a contour mask's geometry through per-point float
// channels (`point{i}.x/.y`). Because CSS only interpolates clip-paths that share the same shape
// function, every sample of one animation yields a `d` with the same verb/point structure, so the
// per-point channels line up across the timeline. Returns '' for `none`, a `url(#id)` reference
// (handled by the static path), or any shape that cannot be resolved. `inset()` rounding is
// ignored (a reveal wipe morphs the rectangle, not its corner radius). This is intentionally
// self-contained so it survives `.toString()` bundling into the capture IIFE.
export function pagxClipNormalizeD(raw: string, el: Element): string {
  const v = (raw || '').trim();
  if (!v || v.toLowerCase() === 'none' || /^url\(/i.test(v)) return '';
  // Measure the element's UNTRANSFORMED layout border box — not getBoundingClientRect(), which
  // reports the box after ancestor CSS transforms. A scaled wrapper (e.g. `.poster{ transform:
  // scale(s) }`) would otherwise make this emit clip geometry in screen pixels while the importer
  // frames the contour mask in the element's unscaled layout box, so the wipe would reveal the
  // wrong region (shifted / cropped by the scale factor). `offsetWidth/offsetHeight` are the
  // border-box size in layout pixels and are immune to transforms. Fall back to the rendered rect
  // for elements without an offset box (e.g. SVG nodes).
  const hostEl = el as HTMLElement;
  let bw = typeof hostEl.offsetWidth === 'number' ? hostEl.offsetWidth : 0;
  let bh = typeof hostEl.offsetHeight === 'number' ? hostEl.offsetHeight : 0;
  if (!(bw > 0) || !(bh > 0)) {
    const rect = el.getBoundingClientRect();
    bw = rect.width;
    bh = rect.height;
  }
  if (!(bw > 0) || !(bh > 0)) return '';
  const cs = getComputedStyle(el);
  const px = (s: string): number => {
    const f = parseFloat(s);
    return isNaN(f) ? 0 : f;
  };
  const bl = px(cs.borderLeftWidth);
  const bt = px(cs.borderTopWidth);
  const brr = px(cs.borderRightWidth);
  const bbb = px(cs.borderBottomWidth);
  const pl = px(cs.paddingLeft);
  const pt = px(cs.paddingTop);
  const pr = px(cs.paddingRight);
  const pb = px(cs.paddingBottom);
  const ml = px(cs.marginLeft);
  const mt = px(cs.marginTop);
  const mr = px(cs.marginRight);
  const mb = px(cs.marginBottom);
  // Split off a trailing geometry-box keyword and resolve the reference rect (origin + size) in
  // border-box coordinates. Everything below is computed in that local box then offset by the box
  // origin so the emitted `d` is always in border-box pixels (matching how the importer frames the
  // contour mask to the element's border box).
  let body = v;
  let ox = 0;
  let oy = 0;
  let w = bw;
  let h = bh;
  const boxMatch = body.match(/\b(border-box|padding-box|content-box|margin-box|fill-box|stroke-box|view-box)\s*$/i);
  if (boxMatch) {
    const box = boxMatch[1].toLowerCase();
    body = body.slice(0, boxMatch.index).trim();
    if (box === 'padding-box') {
      ox = bl; oy = bt; w = bw - bl - brr; h = bh - bt - bbb;
    } else if (box === 'content-box') {
      ox = bl + pl; oy = bt + pt; w = bw - bl - brr - pl - pr; h = bh - bt - bbb - pt - pb;
    } else if (box === 'margin-box') {
      ox = -ml; oy = -mt; w = bw + ml + mr; h = bh + mt + mb;
    }
  }
  if (!(w > 0) || !(h > 0)) return '';
  // Paren-aware split of a shape's argument list on top-level spaces.
  const splitSpace = (s: string): string[] => {
    const out: string[] = [];
    let depth = 0;
    let start = 0;
    for (let i = 0; i < s.length; i++) {
      const c = s[i];
      if (c === '(') depth++;
      else if (c === ')') depth = Math.max(0, depth - 1);
      else if ((c === ' ' || c === '\t' || c === '\n') && depth === 0) {
        if (i > start) out.push(s.slice(start, i));
        start = i + 1;
      }
    }
    if (s.length > start) out.push(s.slice(start));
    return out.filter((t) => t.length > 0);
  };
  // Evaluate a <length-percentage> / calc() token to pixels against `basis`. Handles px, %, bare
  // numbers, and simple calc() with + - * / (left-to-right with * / precedence).
  const evalLen = (tokenRaw: string, basis: number): number => {
    let token = (tokenRaw || '').trim();
    if (!token) return 0;
    const cm = token.match(/^calc\((.*)\)$/i);
    if (cm) token = cm[1];
    // Tokenize into numbers (with optional unit) and operators.
    const toks: string[] = [];
    let i = 0;
    while (i < token.length) {
      const c = token[i];
      if (c === ' ' || c === '\t' || c === '\n') { i++; continue; }
      if (c === '+' || c === '-' || c === '*' || c === '/' || c === '(' || c === ')') {
        toks.push(c); i++; continue;
      }
      let j = i;
      while (j < token.length && !'+-*/() \t\n'.includes(token[j])) j++;
      toks.push(token.slice(i, j)); i = j;
    }
    const val = (t: string): number => {
      if (t.endsWith('%')) return (parseFloat(t) / 100) * basis;
      if (t.endsWith('px')) return parseFloat(t);
      const f = parseFloat(t);
      return isNaN(f) ? 0 : f;
    };
    // Shunting-ish: two-pass, handle * / first then + -. No parentheses expected in resolved
    // computed values, but tolerate them by flattening (ignored).
    const nums: number[] = [];
    const ops: string[] = [];
    for (const t of toks) {
      if (t === '(' || t === ')') continue;
      if (t === '+' || t === '-' || t === '*' || t === '/') ops.push(t);
      else nums.push(val(t));
    }
    if (nums.length === 0) return 0;
    // Fold * and / first.
    const n2: number[] = [nums[0]];
    const o2: string[] = [];
    let k = 0;
    for (let m = 0; m < ops.length; m++) {
      const op = ops[m];
      const rhs = nums[m + 1] != null ? nums[m + 1] : 0;
      if (op === '*') n2[n2.length - 1] *= rhs;
      else if (op === '/') n2[n2.length - 1] /= rhs;
      else { o2.push(op); n2.push(rhs); }
      k = m;
    }
    void k;
    let acc = n2[0];
    for (let m = 0; m < o2.length; m++) {
      acc = o2[m] === '-' ? acc - n2[m + 1] : acc + n2[m + 1];
    }
    return acc;
  };
  const num = (x: number): string => String(Math.round(x * 1000) / 1000);
  const P = (x: number, y: number): string => num(ox + x) + ' ' + num(oy + y);
  const inner = (name: string): string | null => {
    const idx = body.toLowerCase().indexOf(name + '(');
    if (idx < 0) return null;
    const open = body.indexOf('(', idx);
    let depth = 0;
    for (let i = open; i < body.length; i++) {
      if (body[i] === '(') depth++;
      else if (body[i] === ')') { depth--; if (depth === 0) return body.slice(open + 1, i); }
    }
    return null;
  };
  const kappa = 0.5522847498307936;
  const ellipseD = (cx: number, cy: number, rx: number, ry: number): string => {
    return 'M ' + P(cx + rx, cy) +
      ' C ' + P(cx + rx, cy + ry * kappa) + ', ' + P(cx + rx * kappa, cy + ry) + ', ' + P(cx, cy + ry) +
      ' C ' + P(cx - rx * kappa, cy + ry) + ', ' + P(cx - rx, cy + ry * kappa) + ', ' + P(cx - rx, cy) +
      ' C ' + P(cx - rx, cy - ry * kappa) + ', ' + P(cx - rx * kappa, cy - ry) + ', ' + P(cx, cy - ry) +
      ' C ' + P(cx + rx * kappa, cy - ry) + ', ' + P(cx + rx, cy - ry * kappa) + ', ' + P(cx + rx, cy) +
      ' Z';
  };
  // Resolve a CSS <position> (1-2 tokens after `at`) to [cx, cy] in local box coords.
  const resolvePos = (tokens: string[]): [number, number] => {
    let cx = w / 2;
    let cy = h / 2;
    const xk: Record<string, number> = { left: 0, center: w / 2, right: w };
    const yk: Record<string, number> = { top: 0, center: h / 2, bottom: h };
    if (tokens.length >= 1) {
      const t = tokens[0].toLowerCase();
      cx = t in xk ? xk[t] : evalLen(tokens[0], w);
    }
    if (tokens.length >= 2) {
      const t = tokens[1].toLowerCase();
      cy = t in yk ? yk[t] : evalLen(tokens[1], h);
    }
    return [cx, cy];
  };
  // polygon()
  const poly = inner('polygon');
  if (poly != null) {
    let pts = poly.trim();
    // optional fill-rule prefix
    const fr = pts.match(/^(nonzero|evenodd)\s*,\s*/i);
    if (fr) pts = pts.slice(fr[0].length);
    const verts = pagxSplitTopLevelCommas(pts).filter((s) => s.trim().length > 0);
    if (verts.length < 3) return '';
    const parts: string[] = [];
    for (let i = 0; i < verts.length; i++) {
      const xy = splitSpace(verts[i].trim());
      const x = evalLen(xy[0] || '0', w);
      const y = evalLen(xy[1] || '0', h);
      parts.push((i === 0 ? 'M ' : 'L ') + P(x, y));
    }
    return parts.join(' ') + ' Z';
  }
  // inset()
  const ins = inner('inset');
  if (ins != null) {
    let s = ins.trim();
    const ri = s.toLowerCase().indexOf(' round ');
    if (ri >= 0) s = s.slice(0, ri);
    const t = splitSpace(s);
    const top = evalLen(t[0] || '0', h);
    const right = t.length > 1 ? evalLen(t[1], w) : top;
    const bottom = t.length > 2 ? evalLen(t[2], h) : top;
    const left = t.length > 3 ? evalLen(t[3], w) : right;
    const x1 = left;
    const y1 = top;
    const x2 = Math.max(x1, w - right);
    const y2 = Math.max(y1, h - bottom);
    return 'M ' + P(x1, y1) + ' L ' + P(x2, y1) + ' L ' + P(x2, y2) + ' L ' + P(x1, y2) + ' Z';
  }
  // circle()
  const cir = inner('circle');
  if (cir != null) {
    const t = splitSpace(cir.trim());
    let radTok = '';
    let posTokens: string[] = [];
    const atIdx = t.findIndex((x) => x.toLowerCase() === 'at');
    if (atIdx >= 0) {
      radTok = t.slice(0, atIdx).join(' ').trim();
      posTokens = t.slice(atIdx + 1);
    } else {
      radTok = t.join(' ').trim();
    }
    const [cx, cy] = resolvePos(posTokens);
    const diag = Math.sqrt(w * w + h * h) / Math.SQRT2;
    let r: number;
    const rk = radTok.toLowerCase();
    if (!radTok || rk === 'closest-side') {
      r = Math.min(cx, w - cx, cy, h - cy);
    } else if (rk === 'farthest-side') {
      r = Math.max(cx, w - cx, cy, h - cy);
    } else if (rk === 'closest-corner') {
      const dx = Math.min(cx, w - cx);
      const dy = Math.min(cy, h - cy);
      r = Math.sqrt(dx * dx + dy * dy);
    } else if (rk === 'farthest-corner') {
      const dx = Math.max(cx, w - cx);
      const dy = Math.max(cy, h - cy);
      r = Math.sqrt(dx * dx + dy * dy);
    } else {
      r = evalLen(radTok, diag);
    }
    if (!(r >= 0)) r = 0;
    return ellipseD(cx, cy, r, r);
  }
  // ellipse()
  const ell = inner('ellipse');
  if (ell != null) {
    const t = splitSpace(ell.trim());
    let posTokens: string[] = [];
    let radToks: string[] = [];
    const atIdx = t.findIndex((x) => x.toLowerCase() === 'at');
    if (atIdx >= 0) {
      radToks = t.slice(0, atIdx);
      posTokens = t.slice(atIdx + 1);
    } else {
      radToks = t;
    }
    const [cx, cy] = resolvePos(posTokens);
    const sideRx = (kw: string): number =>
      kw === 'farthest-side' ? Math.max(cx, w - cx) : Math.min(cx, w - cx);
    const sideRy = (kw: string): number =>
      kw === 'farthest-side' ? Math.max(cy, h - cy) : Math.min(cy, h - cy);
    const rxTok = (radToks[0] || '').toLowerCase();
    const ryTok = (radToks[1] || '').toLowerCase();
    const rx = !radToks[0] || rxTok === 'closest-side' || rxTok === 'farthest-side'
      ? sideRx(rxTok || 'closest-side') : evalLen(radToks[0], w);
    const ry = !radToks[1] || ryTok === 'closest-side' || ryTok === 'farthest-side'
      ? sideRy(ryTok || 'closest-side') : evalLen(radToks[1], h);
    return ellipseD(cx, cy, Math.max(0, rx), Math.max(0, ry));
  }
  // path()
  const pth = inner('path');
  if (pth != null) {
    const m = pth.match(/(['"])((?:\\.|(?!\1).)*)\1/);
    if (m && m[2]) return m[2].trim();
    return '';
  }
  return '';
}

export function pagxWhichVary(samples: Array<Record<string, string | null>>): string[] {
  const props = ['opacity', 'transform', 'color', 'background-color', 'filter', 'clip-path', 'fill',
    'stroke', 'stroke-dashoffset'];
  const out: string[] = [];
  for (const p of props) {
    let varies = false;
    const base = samples[0][p];
    for (let i = 1; i < samples.length; i++) {
      if (samples[i][p] !== base) {
        varies = true;
        break;
      }
    }
    if (varies) out.push(p);
  }
  return out;
}

// Drop interior keyframes that are identical to both neighbours (flat runs),
// keeping the first and last stop. Compact output without altering the curve.
export function pagxReduceKeyframes(norm: PagxAnimStop[]): PagxAnimStop[] {
  if (norm.length <= 2) return norm;
  const out = [norm[0]];
  for (let i = 1; i < norm.length - 1; i++) {
    const prev = JSON.stringify(out[out.length - 1].props);
    const cur = JSON.stringify(norm[i].props);
    const next = JSON.stringify(norm[i + 1].props);
    if (cur === prev && cur === next) continue; // flat run — drop interior point
    out.push(norm[i]);
  }
  out.push(norm[norm.length - 1]);
  return out;
}

// ===== Keyframe decimation (Ramer–Douglas–Peucker) =====
//
// The rAF sampler (GSAP / anime.js) seeks the timeline at many evenly-spaced
// points and reads `getComputedStyle`, producing one keyframe per sample with
// *linear* interpolation between them. Two opposing pressures:
//   - too few samples → an eased curve becomes a visibly faceted polyline;
//   - too many samples → every channel carries a keyframe per frame, bloating
//     the PAGX file (a page with dozens of simple tweens explodes).
// RDP resolves both generically: sample densely, then drop every interior
// keyframe that lies within `eps` of the straight chord its neighbours already
// describe. A constant-velocity slide collapses to its two endpoints; an eased
// curve keeps only the few points needed to stay within tolerance; the cost is
// bounded by the curve's actual complexity, not the sample count or the page's
// animation count. This keeps linear interpolation (no importer change) and
// only ever runs on the lossy sampled path — the declarative WAAPI / CSS paths
// keep their authored keyframes + easing untouched.

// Parse the r/g/b/a components of an `rgb()` / `rgba()` string. Returns null for
// any other shape (named colors, hex — `getComputedStyle` always serialises to
// rgb/rgba, so those are the only cases that reach the sampler).
export function pagxParseColorChannels(value: string): number[] | null {
  const m = (value || '').match(/rgba?\(([^)]+)\)/i);
  if (!m) return null;
  const parts = m[1].split(',').map((s) => parseFloat(s.trim()));
  if (parts.length < 3 || parts.slice(0, 3).some((x) => isNaN(x))) return null;
  const a = parts.length >= 4 && !isNaN(parts[3]) ? parts[3] : 1;
  return [parts[0], parts[1], parts[2], a];
}

// Decompose a computed `filter` string into the scalar channels the RDP
// decimator tracks, so a glow / shadow / blur animation keeps its curve instead
// of being flattened. `getComputedStyle().filter` is either `none` or a chain of
// space-separated functions (`drop-shadow(rgb(...) 0px 0px 16px) blur(4px) …`).
// We fold every drop-shadow onto ONE representative shadow — the one with the
// largest `blur * alpha` (the dominant glow) — and every `blur()` onto a single
// radius, matching how the importer lowers the chain onto a single animatable
// DropShadowFilter / BlurFilter. `none` (or no recognised function) yields all
// zeros so a none <-> shadow transition reads as an alpha / blur ramp from 0.
// Unrecognised functions (brightness, contrast, …) are ignored (dropped, as the
// runtime has no channel for them).
export function pagxParseFilterChannels(value: string): {
  fdx: number; fdy: number; fdb: number;
  fdr: number; fdg: number; fdbl: number; fda: number;
  fblur: number;
} {
  const zero = { fdx: 0, fdy: 0, fdb: 0, fdr: 0, fdg: 0, fdbl: 0, fda: 0, fblur: 0 };
  const s = (value || '').trim();
  if (!s || s.toLowerCase() === 'none') return zero;
  const out = { ...zero };
  let bestScore = -1;
  let fblur = 0;
  // Paren-aware walk of `name( args )` functions (drop-shadow args themselves
  // contain `rgb(...)`, so a naive regex would mis-split them).
  let i = 0;
  while (i < s.length) {
    while (i < s.length && /\s/.test(s[i])) i++;
    const open = s.indexOf('(', i);
    if (open < 0) break;
    const name = s.slice(i, open).trim().toLowerCase();
    let depth = 1;
    let j = open + 1;
    for (; j < s.length && depth > 0; j++) {
      if (s[j] === '(') depth++;
      else if (s[j] === ')') depth--;
    }
    const args = s.slice(open + 1, j - 1).trim();
    if (name === 'drop-shadow') {
      const color = pagxParseColorChannels(args) || [0, 0, 0, 1];
      // Strip the color token, then read the remaining lengths in order:
      // offsetX offsetY [blur].
      const rest = args.replace(/rgba?\([^)]*\)/i, ' ');
      const nums = (rest.match(/-?[\d.]+/g) || []).map((n) => parseFloat(n));
      const ox = nums[0] || 0;
      const oy = nums[1] || 0;
      const blur = nums[2] || 0;
      const score = blur * (color[3] != null ? color[3] : 1);
      if (score >= bestScore) {
        bestScore = score;
        out.fdx = ox; out.fdy = oy; out.fdb = blur;
        out.fdr = color[0]; out.fdg = color[1]; out.fdbl = color[2];
        out.fda = color[3] != null ? color[3] : 1;
      }
    } else if (name === 'blur') {
      const m = args.match(/-?[\d.]+/);
      if (m) fblur = Math.max(fblur, parseFloat(m[0]));
    }
    i = j;
  }
  out.fblur = fblur;
  return out;
}

// Parse the x/y pixels of a normalised `translate(xpx, ypx)` string (the shape
// `pagxExtractTranslate` emits). Returns null when no x value parses.
export function pagxParseTranslateXY(value: string): number[] | null {
  const m = (value || '').match(/translate\(([^)]+)\)/i);
  if (!m) return null;
  const parts = m[1].split(',').map((s) => parseFloat(s.trim()));
  if (parts.length === 0 || isNaN(parts[0])) return null;
  const y = parts.length >= 2 && !isNaN(parts[1]) ? parts[1] : 0;
  return [parts[0], y];
}

// Decompose the tracked props of every stop into scalar channel series (one
// number per stop). opacity → 1 dim, transform → tx/ty, color/background-color
// → r/g/b/a. Missing samples are left as NaN and carried by the caller.
export function pagxStopScalarSeries(stops: PagxAnimStop[]): Record<string, number[]> {
  const series: Record<string, number[]> = {};
  const put = (key: string, i: number, v: number): void => {
    if (!series[key]) series[key] = new Array(stops.length).fill(NaN);
    series[key][i] = v;
  };
  for (let i = 0; i < stops.length; i++) {
    const p = stops[i].props || {};
    if (p['opacity'] != null) {
      const o = parseFloat(p['opacity']);
      if (!isNaN(o)) put('opacity', i, o);
    }
    if (p['transform'] != null) {
      // Decompose to the six affine components so RDP sees scale / rotation /
      // skew curvature, not just translation. A pure-translate value folds to
      // [1,0,0,1,tx,ty]: the constant linear channels collapse to endpoints and
      // m4/m5 behave exactly like the old tx/ty channels, so translate-only
      // output is unchanged while scale/rotate keyframes now survive.
      const mtx = pagxTransformToMatrix6(p['transform']);
      if (mtx) {
        put('m0', i, mtx[0]);
        put('m1', i, mtx[1]);
        put('m2', i, mtx[2]);
        put('m3', i, mtx[3]);
        put('m4', i, mtx[4]);
        put('m5', i, mtx[5]);
      }
    }
    if (p['color'] != null) {
      const c = pagxParseColorChannels(p['color']);
      if (c) {
        put('cr', i, c[0]);
        put('cg', i, c[1]);
        put('cb', i, c[2]);
        put('ca', i, c[3]);
      }
    }
    if (p['background-color'] != null) {
      const c = pagxParseColorChannels(p['background-color']);
      if (c) {
        put('br', i, c[0]);
        put('bg', i, c[1]);
        put('bb', i, c[2]);
        put('ba', i, c[3]);
      }
    }
    if (p['fill'] != null) {
      const c = pagxParseColorChannels(p['fill']);
      if (c) {
        put('flr', i, c[0]);
        put('flg', i, c[1]);
        put('flb', i, c[2]);
        put('fla', i, c[3]);
      }
    }
    if (p['stroke'] != null) {
      const c = pagxParseColorChannels(p['stroke']);
      if (c) {
        put('skr', i, c[0]);
        put('skg', i, c[1]);
        put('skb', i, c[2]);
        put('ska', i, c[3]);
      }
    }
    if (p['stroke-dashoffset'] != null) {
      const v = parseFloat(p['stroke-dashoffset']);
      if (!isNaN(v)) put('sdo', i, v);
    }
    if (p['filter'] != null) {
      const f = pagxParseFilterChannels(p['filter']);
      put('fdx', i, f.fdx);
      put('fdy', i, f.fdy);
      put('fdb', i, f.fdb);
      put('fdr', i, f.fdr);
      put('fdg', i, f.fdg);
      put('fdbl', i, f.fdbl);
      put('fda', i, f.fda);
      put('fblur', i, f.fblur);
    }
    // clip-path is emitted as `path("<d>")`; expose each coordinate number as its own scalar
    // channel (cp0, cp1, ...) so RDP keeps the frames where the mask geometry actually moves. All
    // stops of one animation share the same shape structure, so coordinate index k is comparable
    // across stops.
    if (p['clip-path'] != null && p['clip-path']) {
      const dm = p['clip-path'].match(/path\(\s*(['"])((?:\\.|(?!\1).)*)\1\s*\)/i);
      if (dm && dm[2]) {
        const nums = dm[2].match(/-?\d*\.?\d+(?:e[-+]?\d+)?/gi);
        if (nums) {
          for (let k = 0; k < nums.length; k++) {
            const val = parseFloat(nums[k]);
            if (!isNaN(val)) put('cp' + k, i, val);
          }
        }
      }
    }
  }
  return series;
}

// RDP on one scalar channel. `offsets` and `values` are parallel; values are
// normalised to their own [0,1] travel so `eps` is a scale-free fraction of the
// channel's total motion (a 2px wiggle and a 200px slide are simplified to the
// same relative tolerance). Returns a per-index keep mask. A flat channel
// (zero travel) keeps only its endpoints.
export function pagxRdpKeep(offsets: number[], values: number[], eps: number, absEps = 0): boolean[] {
  const n = values.length;
  const keep = new Array(n).fill(false);
  if (n === 0) return keep;
  keep[0] = true;
  keep[n - 1] = true;
  if (n <= 2) return keep;
  let lo = Infinity;
  let hi = -Infinity;
  for (const v of values) {
    if (v < lo) lo = v;
    if (v > hi) hi = v;
  }
  const range = hi - lo;
  if (!(range > 0)) return keep; // flat channel — endpoints only
  // Because distances are measured on the value axis normalised to this
  // channel's own [0,1] travel, `eps` is a fraction of the *total* motion — so
  // a small-but-visible move loses to a larger one sharing the channel. A ±2px
  // "shake" on an element that also slides 260px elsewhere on the timeline is
  // only ~0.8% of the travel and falls under a 1% `eps`, erasing the shake.
  // `absEps` (in the channel's own value units, e.g. px for translation) sets a
  // floor in absolute terms: on a wide-range channel it tightens the effective
  // tolerance to `absEps / range` so motion above the floor is kept regardless
  // of how far the channel travels elsewhere. Zero (the default) preserves the
  // pure relative behaviour for channels where an absolute unit is meaningless
  // (opacity, colour, matrix scale/rotation).
  const effEps = absEps > 0 ? Math.min(eps, absEps / range) : eps;
  const norm = values.map((v) => (v - lo) / range);
  const stack: number[][] = [[0, n - 1]];
  while (stack.length) {
    const seg = stack.pop() as number[];
    const a = seg[0];
    const b = seg[1];
    if (b <= a + 1) continue;
    const x1 = offsets[a];
    const y1 = norm[a];
    const dx = offsets[b] - x1;
    const dy = norm[b] - y1;
    const denom = Math.sqrt(dx * dx + dy * dy) || 1;
    let maxD = -1;
    let idx = -1;
    for (let i = a + 1; i < b; i++) {
      const d = Math.abs(dy * (offsets[i] - x1) - dx * (norm[i] - y1)) / denom;
      if (d > maxD) {
        maxD = d;
        idx = i;
      }
    }
    if (maxD > effEps && idx > a && idx < b) {
      keep[idx] = true;
      stack.push([a, idx]);
      stack.push([idx, b]);
    }
  }
  return keep;
}

// Decimate sampled stops: run RDP independently per scalar channel and keep the
// union of the points any channel needs (plus both endpoints). `eps` defaults
// to 1% of each channel's travel — tight enough to preserve eased curvature
// (looser values visibly under-fit fast counters / slides) while still
// collapsing constant-velocity runs to their endpoints. The literal default
// travels with the function source when it is serialised into the page. No-op
// for <= 2 stops.
//
// Pixel-valued channels additionally get an absolute sub-pixel floor
// (`ABS_FLOOR_PX`): a small motion (e.g. a ±2px "shake" retriggered while an
// element also slides hundreds of px elsewhere on the timeline) is otherwise
// erased by the relative `eps` because it is a tiny fraction of the channel's
// full travel. Sub-pixel motion is imperceptible, so the floor keeps anything
// above it. Dimensionless channels (opacity, matrix scale/rotation m0-m3,
// colour) pass 0 and keep the pure relative tolerance.
export function pagxDecimateStops(stops: PagxAnimStop[], eps = 0.01): PagxAnimStop[] {
  if (stops.length <= 2) return stops;
  // Sub-pixel motion is imperceptible; keep anything above this on pixel-valued
  // channels even when a larger move on the same channel would mask it under
  // the relative eps. Inlined literal so it survives function-source bundling.
  const ABS_FLOOR_PX = 0.5;
  const offsets = stops.map((s) => s.offset);
  const series = pagxStopScalarSeries(stops);
  const keep = new Array(stops.length).fill(false);
  keep[0] = true;
  keep[stops.length - 1] = true;
  for (const key of Object.keys(series)) {
    const vals = series[key];
    // Translation (matrix e/f = m4/m5), drop-shadow offset/blur, and clip-path
    // coordinates (cp0, cp1, …) are all measured in CSS pixels, so they get the
    // absolute floor; every other channel keeps the pure relative tolerance.
    const absEps =
      key === 'm4' || key === 'm5' ||
      key === 'fdx' || key === 'fdy' || key === 'fdb' || key === 'fblur' ||
      (key[0] === 'c' && key[1] === 'p')
        ? ABS_FLOOR_PX
        : 0;
    // Defensive NaN fill (dense sampling is normally hole-free): carry the last
    // seen value forward, then backfill any leading gap, so RDP sees a complete
    // series.
    let last = NaN;
    for (let i = 0; i < vals.length; i++) {
      if (isNaN(vals[i])) vals[i] = last;
      else last = vals[i];
    }
    let next = NaN;
    for (let i = vals.length - 1; i >= 0; i--) {
      if (isNaN(vals[i])) vals[i] = next;
      else next = vals[i];
    }
    const k = pagxRdpKeep(offsets, vals, eps, absEps);
    for (let i = 0; i < k.length; i++) {
      if (k[i]) keep[i] = true;
    }
  }
  const out: PagxAnimStop[] = [];
  for (let i = 0; i < stops.length; i++) {
    if (keep[i]) out.push(stops[i]);
  }
  return out;
}

export function pagxNormalizeTiming(t: string): string {
  const v = (t || '').trim();
  if (!v) return 'linear';
  return v;
}

// Paren-aware comma split for CSS list values such as
// `cubic-bezier(0.42, 0, 0.58, 1), linear` — splitting on a naive `,` would
// shatter `cubic-bezier(...)` into pieces.
export function pagxSplitTopLevelCommas(value: string): string[] {
  const out: string[] = [];
  let depth = 0;
  let start = 0;
  for (let i = 0; i < value.length; i++) {
    const c = value[i];
    if (c === '(') depth++;
    else if (c === ')') depth = Math.max(0, depth - 1);
    else if (c === ',' && depth === 0) {
      out.push(value.slice(start, i).trim());
      start = i + 1;
    }
  }
  out.push(value.slice(start).trim());
  return out;
}

// Convert a computed `box-shadow` string into an equivalent `drop-shadow(...)`
// filter chain, so a glow that only appears in an animated state (e.g. a `.on`
// class toggled by JS over the timeline) rides the runtime's animatable filter
// channel.
//
// The capture pipeline only plays back opacity / transform / color /
// background-color / filter (see pagxWhichVary): `box-shadow` has no animation
// channel, so a shadow that toggles across the timeline silently drops its glow
// (only a co-declared `opacity` survives). CSS `box-shadow` and
// `filter: drop-shadow()` are visually equivalent for the zero-spread outer
// glows these UIs use, and the importer already lowers an animated
// `filter: drop-shadow(...)` onto a DropShadowFilter — so folding the shadow into
// the filter channel is all that is needed. Static box-shadows are unaffected:
// a shadow present in every sample stays constant on the filter channel and is
// dropped by pagxWhichVary, leaving the static snapshot path to paint it.
//
// Computed form is `<color> <offX> <offY> <blur> [<spread>]` per shadow (comma
// separated). `inset` shadows have no drop-shadow analogue and are skipped;
// `spread` is dropped (the importer ignores it anyway). Returns the space-joined
// `drop-shadow(...)` chain, or null when nothing convertible remains.
export function pagxBoxShadowToFilter(boxShadow: string | null | undefined): string | null {
  const raw = (boxShadow || '').trim();
  if (!raw || raw.toLowerCase() === 'none') return null;
  const parts: string[] = [];
  for (const item of pagxSplitTopLevelCommas(raw)) {
    const spec = item.trim();
    if (!spec) continue;
    // Top-level whitespace tokeniser that keeps `rgb(...)` / `rgba(...)` intact.
    const tokens: string[] = [];
    let depth = 0;
    let cur = '';
    for (let i = 0; i < spec.length; i++) {
      const c = spec[i];
      if (c === '(') depth++;
      else if (c === ')') depth = Math.max(0, depth - 1);
      if (depth === 0 && /\s/.test(c)) {
        if (cur) {
          tokens.push(cur);
          cur = '';
        }
      } else {
        cur += c;
      }
    }
    if (cur) tokens.push(cur);
    let inset = false;
    const lengths: string[] = [];
    const colors: string[] = [];
    for (const t of tokens) {
      if (t.toLowerCase() === 'inset') {
        inset = true;
        continue;
      }
      if (/^-?[\d.]+(px)?$/.test(t)) lengths.push(t);
      else colors.push(t);
    }
    if (inset) continue; // inner shadow: no drop-shadow analogue
    if (lengths.length < 2) continue; // need at least offX / offY
    const offX = lengths[0];
    const offY = lengths[1];
    const blur = lengths.length >= 3 ? lengths[2] : '0px';
    const color = colors.length ? colors.join(' ') : 'rgb(0, 0, 0)';
    parts.push('drop-shadow(' + offX + ' ' + offY + ' ' + blur + ' ' + color + ')');
  }
  return parts.length ? parts.join(' ') : null;
}

// Resolve the effective CSS-style timing-function for a WAAPI animation.
//
// `getAnimations()` returns three flavours:
//   1. `CSSAnimation` — created by a stylesheet `@keyframes` + `animation-*`
//      declarations. The active timing-function lives on the element's
//      computed `animation-timing-function`; per-keyframe `easing` and
//      `effect.getTiming().easing` both default to `linear` and would lose
//      the author's `ease-in-out` / `cubic-bezier(...)` etc.
//   2. WAAPI animations from `element.animate(...)` — the easing is on
//      `effect.getTiming().easing` (effect-level) or per keyframe.
//   3. Motion One / web-animations-js — also surface via WAAPI, easing is
//      on `effect.getTiming().easing`.
//
// Priority: if `anim` is a `CSSAnimation` (carries `animationName`), trust
// computed style for that name. Otherwise read effect-level easing, falling
// back to `ct.easing` and finally `linear`. The function never throws —
// callers (in-page IIFE) cannot afford to abort the whole capture if one
// engine surfaces unexpectedly-shaped objects.
export function pagxResolveWaapiEasing(
  anim: unknown,
  effect: unknown,
  ct: { easing?: string },
  target: HTMLElement,
): string {
  try {
    const cssName = (anim as { animationName?: string }).animationName;
    if (cssName) {
      const cs = getComputedStyle(target);
      const names = pagxSplitTopLevelCommas(cs.animationName || '');
      const idx = names.indexOf(cssName);
      if (idx >= 0) {
        const timings = pagxSplitTopLevelCommas(cs.animationTimingFunction || '');
        const picked = timings[idx] || timings[0] || '';
        if (picked) return picked;
      }
    }
  } catch (_) {
    /* fall through to WAAPI-effect lookup */
  }
  try {
    const effectTiming = (effect as { getTiming?: () => { easing?: string } }).getTiming;
    const fromEffect = effectTiming ? effectTiming.call(effect).easing : '';
    return fromEffect || (ct && ct.easing) || 'linear';
  } catch (_) {
    return 'linear';
  }
}

// Pure: turn a captured descriptor into its canonical `@keyframes` body and the
// matching `animation` shorthand value (named `<prefix><index>`). Returns null
// when no subset declarations survive (so the caller can skip it). Shared by the
// in-page emitter and the unit tests.
export function pagxBuildCanonicalAnimation(
  cap: PagxAnimDescriptor,
  index: number,
  prefix: string,
): { name: string; keyframesCss: string; animationShorthand: string } | null {
  const stops = cap.keyframes.slice().sort((a, b) => a.offset - b.offset);
  const lines: string[] = [];
  for (const s of stops) {
    const decls: string[] = [];
    if (s.props['opacity'] != null) decls.push('opacity: ' + s.props['opacity']);
    if (s.props['transform'] != null) decls.push('transform: ' + s.props['transform']);
    if (s.props['color'] != null) decls.push('color: ' + s.props['color']);
    if (s.props['background-color'] != null) {
      decls.push('background-color: ' + s.props['background-color']);
    }
    if (s.props['filter'] != null) decls.push('filter: ' + s.props['filter']);
    if (s.props['clip-path'] != null && s.props['clip-path']) {
      decls.push('clip-path: ' + s.props['clip-path']);
    }
    if (s.props['fill'] != null) decls.push('fill: ' + s.props['fill']);
    if (s.props['stroke'] != null) decls.push('stroke: ' + s.props['stroke']);
    if (s.props['stroke-dashoffset'] != null) {
      decls.push('stroke-dashoffset: ' + s.props['stroke-dashoffset']);
    }
    if (decls.length === 0) continue;
    lines.push((Math.round(s.offset * 1000) / 10) + '% { ' + decls.join('; ') + '; }');
  }
  if (lines.length === 0) return null;
  const name = prefix + index;
  const keyframesCss = '@keyframes ' + name + ' { ' + lines.join(' ') + ' }';
  const durSec = cap.durationMs / 1000;
  const delaySec = (cap.delayMs || 0) / 1000;
  const infinite =
    cap.iterations === Infinity || !isFinite(cap.iterations as number) ||
    String(cap.iterations) === 'infinite';
  const iter = infinite ? 'infinite' : cap.iterations || 1;
  const dir = cap.direction || 'normal';
  // `both` so the element holds the 0% keyframe state during `animation-delay`
  // *and* its 100% keyframe state after a finite iteration finishes, instead
  // of CSS's default `none` (which would show the inline base style during
  // the delay and again after the iteration finishes — collapsing every
  // animated element to its final visual state immediately and freezing the
  // timeline). Using `both` rather than the narrower `backwards` matters
  // specifically for the eval-animation harness: under `fill: none` /
  // `backwards`, a finite animation drops out of `document.getAnimations()`
  // once it crosses its end time, so a seek loop that pauses every animation
  // and then drives `currentTime` finds nothing to drive on the post-active
  // frames — the element re-collapses to its inline base for the rest of the
  // sample grid. `both` keeps each finite animation alive in
  // `getAnimations()` indefinitely so the harness can seek it to any point on
  // the global timeline (matches the original page's CSS animations, which
  // either authored `forwards` / `both` themselves or remain in
  // `getAnimations()` because they never finish during the harness window).
  // See spec/html_subset.md §13.
  const animationShorthand =
    name + ' ' + durSec + 's ' + pagxNormalizeTiming(cap.timing) + ' ' +
    delaySec + 's ' + iter + ' ' + dir + ' both';
  return { name, keyframesCss, animationShorthand };
}

// Pure: build a 2-stop descriptor for a captured CSS `transition` from its
// recorded start (`fromBag`) and resting end (`toBag`) property bags. Both bags
// are raw (un-normalised) property maps in the shape `pagxNormalizeProps`
// accepts; `box` resolves percent translations. Returns null when the
// transition produces no playable motion (zero duration, or no tracked channel
// whose value actually changes between the two states), so the caller can skip
// it.
//
// A CSS transition is not declarative like `@keyframes`: it only describes how
// to interpolate when a property changes. We reconstruct the equivalent
// keyframe animation from the two endpoints we observed — the value at the
// moment the transition started (recorded via the `transitionrun` listener
// installed by `pagxTransitionInstall`) and the value once the page has settled
// (read at capture time). A `transform` channel present on only one side is
// treated as `translate(0px, 0px)` on the missing side (a `none -> translate()`
// or `translate() -> none` transition still carries motion); other channels
// require both endpoints, since e.g. an opacity defined on only one side cannot
// describe a tween.
export function pagxTransitionDescriptorFromBags(
  fromBag: Record<string, unknown>,
  toBag: Record<string, unknown>,
  box: { width: number; height: number } | null,
  durationMs: number,
  delayMs: number,
  timing: string,
): PagxAnimDescriptor | null {
  if (!(durationMs > 0)) return null;
  const fromNorm = pagxNormalizeProps(fromBag, box);
  const toNorm = pagxNormalizeProps(toBag, box);
  const keys: Record<string, true> = {};
  for (const k of Object.keys(fromNorm)) keys[k] = true;
  for (const k of Object.keys(toNorm)) keys[k] = true;
  const startProps: Record<string, string> = {};
  const endProps: Record<string, string> = {};
  let changed = false;
  for (const key of Object.keys(keys)) {
    let fv: string | null = fromNorm[key] != null ? fromNorm[key] : null;
    let tv: string | null = toNorm[key] != null ? toNorm[key] : null;
    if (key === 'transform') {
      if (fv == null) fv = 'translate(0px, 0px)';
      if (tv == null) tv = 'translate(0px, 0px)';
    }
    if (fv == null || tv == null) continue; // single-sided channel — not a tween
    if (fv === tv) continue; // static channel — nothing to animate
    startProps[key] = fv;
    endProps[key] = tv;
    changed = true;
  }
  if (!changed) return null;
  return {
    keyframes: [
      { offset: 0, props: startProps },
      { offset: 1, props: endProps },
    ],
    durationMs,
    delayMs: delayMs || 0,
    iterations: 1,
    direction: 'normal',
    timing: timing || 'linear',
  };
}

// ===== In-browser DOM collectors (module scope, serialised into the page) =====

export function pagxCandidateElements(maxElements: number): HTMLElement[] {
  let list: HTMLElement[] = [];
  try {
    list = Array.prototype.slice.call(document.body.querySelectorAll('*'));
  } catch (_) {
    list = [];
  }
  if (list.length > maxElements) list = list.slice(0, maxElements);
  return list;
}

// Record the initial DOM state (every element's `class` + inline `style`, and
// the set of live nodes) so it can be restored after the global sampler has
// advanced the virtual clock through the timeline. Advancing the clock fires
// the page's timer-driven state machine, which mutates the DOM (toggles scene
// classes, sometimes appends transient nodes like countdown digits); the
// snapshot that follows capture must serialise the *initial* scene (menu
// visible, later panels at opacity 0 with a pagxAnim that fades them in), not
// whatever end state the timeline left behind.
export function pagxDomCheckpoint(): {
  records: Array<{ el: Element; className: string | null; style: string | null }>;
  set: Set<Element>;
} {
  const records: Array<{ el: Element; className: string | null; style: string | null }> = [];
  const set = new Set<Element>();
  let list: Element[] = [];
  try {
    list = Array.prototype.slice.call(document.body.querySelectorAll('*'));
  } catch (_) {
    list = [];
  }
  for (const el of list) {
    set.add(el);
    records.push({
      el,
      className: el.getAttribute('class'),
      style: el.getAttribute('style'),
    });
  }
  return { records, set };
}

// Restore the DOM to a checkpoint taken by pagxDomCheckpoint: drop any node the
// timeline appended after the checkpoint, then restore each original element's
// `class` + inline `style`. This reverts the scene-switching class flips (and
// the inline `animation-delay` a demo sequence may have written on perks, etc.)
// so `pagxEmitCaptured` and the snapshot walker see the initial scene. Nodes
// that were detached by the timeline (rare for the class-toggle idiom) are left
// alone — there is nothing to reattach them to.
export function pagxDomRestore(cp: {
  records: Array<{ el: Element; className: string | null; style: string | null }>;
  set: Set<Element>;
} | null): void {
  if (!cp) return;
  try {
    let now: Element[] = [];
    try {
      now = Array.prototype.slice.call(document.body.querySelectorAll('*'));
    } catch (_) {
      now = [];
    }
    for (const el of now) {
      if (!cp.set.has(el) && el.parentNode) {
        try { el.parentNode.removeChild(el); } catch (_) { /* ignore */ }
      }
    }
  } catch (_) {
    /* ignore */
  }
  for (const r of cp.records) {
    try {
      if ((r.el as { isConnected?: boolean }).isConnected === false) continue;
      if (r.className === null) r.el.removeAttribute('class');
      else r.el.setAttribute('class', r.className);
      if (r.style === null) r.el.removeAttribute('style');
      else r.el.setAttribute('style', r.style);
    } catch (_) {
      /* ignore this element */
    }
  }
}

export function pagxBuildKeyframesIndex(): Record<string, unknown> {
  const idx: Record<string, unknown> = {};
  let sheets: StyleSheet[] = [];
  try {
    sheets = Array.prototype.slice.call(document.styleSheets);
  } catch (_) {
    sheets = [];
  }
  for (const sheet of sheets) {
    let rules: unknown = null;
    try {
      rules = (sheet as CSSStyleSheet).cssRules || (sheet as { rules?: unknown }).rules;
    } catch (_) {
      continue; // cross-origin sheet
    }
    if (!rules) continue;
    for (const rule of Array.prototype.slice.call(rules)) {
      if (rule.type === 7 || (rule.name && rule.cssRules)) {
        idx[rule.name] = rule;
      }
    }
  }
  return idx;
}

export function pagxCollectWAAPI(captured: unknown[], seen: Set<Element>): void {
  let anims: Animation[] = [];
  try {
    anims = (document as { getAnimations?: () => Animation[] }).getAnimations
      ? (document as { getAnimations: () => Animation[] }).getAnimations()
      : [];
  } catch (_) {
    anims = [];
  }
  for (const anim of anims) {
    try {
      const effect = (anim as { effect?: unknown }).effect;
      if (!effect || typeof effect.getKeyframes !== 'function') continue;
      const target: Element = effect.target;
      if (!target || target.nodeType !== 1 || seen.has(target)) continue;
      const ct = effect.getComputedTiming();
      const durationMs = typeof ct.duration === 'number' ? ct.duration : 0;
      if (!durationMs || durationMs <= 0) continue;
      const norm: Array<{ offset: number | null; props: Record<string, string> }> = [];
      // Used box dimensions of the animated element. Forwarded to
      // pagxNormalizeProps so percent translates resolve to absolute pixels.
      const tgtRect = (target as HTMLElement).getBoundingClientRect();
      const tgtBox = { width: tgtRect.width, height: tgtRect.height };
      for (const kf of effect.getKeyframes()) {
        const props = pagxNormalizeProps(kf, tgtBox);
        if (Object.keys(props).length === 0) continue;
        const offset = kf.computedOffset != null ? kf.computedOffset : kf.offset;
        norm.push({ offset: offset != null ? offset : null, props });
      }
      if (norm.length === 0) continue;
      pagxFillOffsets(norm);
      const easing = pagxResolveWaapiEasing(anim, effect, ct, target as HTMLElement);
      captured.push({
        el: target as HTMLElement,
        keyframes: norm,
        durationMs,
        delayMs: typeof ct.delay === 'number' ? ct.delay : 0,
        iterations: ct.iterations,
        direction: ct.direction || 'normal',
        timing: easing,
      });
      seen.add(target);
    } catch (_) {
      /* skip this animation */
    }
  }
}

export function pagxCollectCSS(captured: unknown[], seen: Set<Element>, maxElements: number): void {
  const idx = pagxBuildKeyframesIndex();
  if (Object.keys(idx).length === 0) return;
  const all = pagxCandidateElements(maxElements);
  for (const el of all) {
    if (seen.has(el)) continue;
    const cs = getComputedStyle(el);
    const nameRaw = (cs.animationName || '').split(',')[0].trim();
    if (!nameRaw || nameRaw === 'none') continue;
    const rule = idx[nameRaw];
    if (!rule || !rule.cssRules) continue;
    const norm: PagxAnimStop[] = [];
    // Used box dimensions of the animated element. Forwarded to
    // pagxNormalizeProps so author-percent translates (`translate(-50%)`,
    // common for marquee/track idioms) become absolute pixels — the importer
    // rejects percent values in @keyframes and would otherwise drop the
    // entire keyframe transform with a diagnostic.
    const elRect = (el as HTMLElement).getBoundingClientRect();
    const elBox = { width: elRect.width, height: elRect.height };
    for (const kf of Array.prototype.slice.call(rule.cssRules)) {
      const offsets = (kf.keyText || '').split(',');
      const props = pagxNormalizeProps({
        opacity: kf.style.opacity,
        transform: kf.style.transform,
        color: kf.style.color,
        backgroundColor: kf.style.backgroundColor,
        filter: kf.style.filter,
        fill: kf.style.fill,
        stroke: kf.style.stroke,
        strokeDashoffset: kf.style.strokeDashoffset,
      }, elBox);
      if (Object.keys(props).length === 0) continue;
      for (const o of offsets) {
        const off = pagxOffsetFromKeyText(o);
        if (off != null) norm.push({ offset: off, props });
      }
    }
    if (norm.length === 0) continue;
    const durMs = pagxParseTimeMs((cs.animationDuration || '').split(',')[0]);
    if (!durMs) continue;
    captured.push({
      el: el as HTMLElement,
      keyframes: norm,
      durationMs: durMs,
      delayMs: pagxParseTimeMs((cs.animationDelay || '').split(',')[0]) || 0,
      iterations: (cs.animationIterationCount || '').split(',')[0].trim() === 'infinite'
        ? Infinity
        : parseFloat(cs.animationIterationCount) || 1,
      direction: (cs.animationDirection || 'normal').split(',')[0].trim(),
      timing: (cs.animationTimingFunction || 'linear').split(',')[0].trim(),
    });
    seen.add(el);
  }
}

// Group a per-sample text series into contiguous runs of the same *non-empty*
// value. Each run becomes one `{ text, startIdx, endIdx }` segment (inclusive
// sample indices); empty samples are gaps and start no segment. A value that
// recurs in two non-adjacent runs yields two segments, so a counter that shows
// `+1 … +20` and a countdown that shows `3 2 1 GO` both decompose into one
// segment per visible number. Pure so it can be unit-tested off-DOM.
export function pagxTextSegments(
  texts: string[],
): Array<{ text: string; startIdx: number; endIdx: number }> {
  const out: Array<{ text: string; startIdx: number; endIdx: number }> = [];
  let i = 0;
  while (i < texts.length) {
    const t = texts[i];
    if (!t) {
      i++;
      continue;
    }
    let j = i;
    while (j + 1 < texts.length && texts[j + 1] === t) j++;
    out.push({ text: t, startIdx: i, endIdx: j });
    i = j + 1;
  }
  return out;
}

export function pagxSampleTimeline(
  captured: unknown[],
  seen: Set<Element>,
  durationMs: number,
  seekFn: (progress: number) => void,
  iterations: number,
  sampleCount: number,
  maxElements: number,
  direction: string,
  delayMs: number,
  textDynamics?: PagxTextDynamic[],
): void {
  const candidates = pagxCandidateElements(maxElements);
  const series = new Map<HTMLElement, Array<Record<string, string | null>>>();
  // Parallel per-sample record of every text-leaf element (an element with no
  // element children) so a `textContent` that the timeline mutates — a combo
  // counter, a 3-2-1 countdown — can be reconstructed as overlay layers. Only
  // populated when the caller wants it (the global sampler); the library-timeline
  // callers (GSAP / anime) pass no `textDynamics` and skip the extra bookkeeping.
  const wantText = Array.isArray(textDynamics);
  const textLeaf = wantText
    ? new Map<HTMLElement, { texts: string[]; fontSizes: string[]; colors: string[]; fills: string[]; opacities: number[] }>()
    : null;
  // Guard sampleCount === 1 so the offset math (i / (sampleCount - 1)) does not divide by zero.
  // Callers normally pass >= 2, but the function is exported and unit-tested as a pure helper.
  if (sampleCount < 2) sampleCount = 2;
  for (let i = 0; i < sampleCount; i++) {
    const p = i / (sampleCount - 1);
    try {
      seekFn(p);
    } catch (_) {
      /* ignore seek failure */
    }
    void document.body.offsetHeight; // force reflow
    for (const el of candidates) {
      const cs = getComputedStyle(el);
      // `filter` carries glow / shadow / blur animations (e.g. a click "halo"
      // authored as `filter: drop-shadow(...) ...`). Kept verbatim as the
      // computed string; the importer maps a varying drop-shadow / blur onto
      // the runtime's animatable DropShadowFilter / BlurFilter channels. An
      // animated `box-shadow` glow (e.g. a `.on` class toggled by JS) has no
      // channel of its own, so it is folded into the filter channel here as an
      // equivalent drop-shadow (constant shadows stay flat and get dropped by
      // pagxWhichVary — see pagxBoxShadowToFilter).
      const shadowFilter = pagxBoxShadowToFilter(cs.boxShadow);
      let filterVal = cs.filter;
      if (shadowFilter) {
        filterVal = filterVal && filterVal !== 'none'
          ? filterVal + ' ' + shadowFilter
          : shadowFilter;
      }
      // Animated `clip-path` (contour mask): normalize any shape form to a canonical SVG path `d`
      // in border-box pixels so the importer can morph a mask Path's points over time. Only pay the
      // getBoundingClientRect / shape-parse cost when the element actually carries a geometric
      // clip-path (skip `none` and `url(#id)` references, which the static path already handles).
      const rawClip = cs.clipPath;
      const clipD = rawClip && rawClip !== 'none' && !/^url\(/i.test(rawClip)
        ? pagxClipNormalizeD(rawClip, el)
        : '';
      const svgCs = cs as unknown as { fill?: string; stroke?: string; strokeDashoffset?: string };
      const snap: Record<string, string | null> = {
        opacity: cs.opacity,
        transform: pagxExtractTransform(cs.transform),
        color: cs.color,
        'background-color': cs.backgroundColor,
        filter: filterVal,
        'clip-path': clipD ? 'path("' + clipD + '")' : '',
        // SVG paint channels (constant/absent for HTML nodes; dropped by pagxWhichVary there).
        fill: svgCs.fill != null ? svgCs.fill : null,
        stroke: svgCs.stroke != null ? svgCs.stroke : null,
        'stroke-dashoffset': svgCs.strokeDashoffset != null ? svgCs.strokeDashoffset : null,
      };
      let arr = series.get(el);
      if (!arr) {
        arr = [];
        series.set(el, arr);
      }
      arr.push(snap);
      // Only HTML text leaves: an SVG `<text>` (namespace svg) whose glyph the
      // timeline swaps (e.g. a `<text>`-based 3-2-1-GO countdown) cannot be
      // reproduced by overlays — the importer converts inline SVG as a unit and
      // does not animate individual SVG sub-elements as PAG layers, so overlay
      // clones would stack at a constant opacity instead of alternating. Leaving
      // SVG text to the existing static path is strictly better than that.
      if (textLeaf && el.children.length === 0
        && el.namespaceURI !== 'http://www.w3.org/2000/svg') {
        let rec = textLeaf.get(el);
        if (!rec) {
          rec = { texts: [], fontSizes: [], colors: [], fills: [], opacities: [] };
          textLeaf.set(el, rec);
        }
        let txt = '';
        try {
          txt = (el.textContent || '').trim();
        } catch (_) {
          txt = '';
        }
        rec.texts.push(txt);
        rec.fontSizes.push(cs.fontSize);
        rec.colors.push(cs.color);
        rec.fills.push((cs as unknown as { fill?: string }).fill || '');
        rec.opacities.push(parseFloat(cs.opacity) || 0);
      }
    }
  }
  if (textLeaf && textDynamics) {
    // An element is "dynamic text" when some sample carries a non-empty value
    // that differs from its t=0 (sample 0) text — i.e. the timeline scripted
    // the glyph in. Static labels (their text is the same across the window)
    // vary nowhere and are skipped, so this only fires for scripted counters /
    // countdowns and leaves ordinary text untouched.
    for (const [el, rec] of textLeaf) {
      const base = rec.texts[0];
      let dynamic = false;
      for (let i = 1; i < rec.texts.length; i++) {
        if (rec.texts[i] && rec.texts[i] !== base) {
          dynamic = true;
          break;
        }
      }
      if (!dynamic) continue;
      const ranges = pagxTextSegments(rec.texts);
      if (ranges.length === 0) continue;
      const n = rec.texts.length;
      const segments: PagxTextSegment[] = [];
      for (const seg of ranges) {
        // Representative sample = the frame this value is most visible on, used
        // to snapshot the size / color it carried while shown (a combo scales
        // its font and shifts its hue with the count).
        let repIdx = seg.startIdx;
        let best = -1;
        for (let i = seg.startIdx; i <= seg.endIdx; i++) {
          if (rec.opacities[i] > best) {
            best = rec.opacities[i];
            repIdx = i;
          }
        }
        // Opacity over the whole window: the real sampled fade inside the
        // segment, 0 outside, so the overlay pops in and out at the exact
        // global times the baseline shows this value. RDP trims the flat runs.
        let stops: PagxAnimStop[] = [];
        for (let i = 0; i < n; i++) {
          const inSeg = i >= seg.startIdx && i <= seg.endIdx;
          const op = inSeg ? rec.opacities[i] : 0;
          stops.push({ offset: n > 1 ? i / (n - 1) : 0, props: { opacity: String(op) } });
        }
        stops = pagxDecimateStops(stops);
        segments.push({
          text: seg.text,
          fontSize: rec.fontSizes[repIdx],
          color: rec.colors[repIdx],
          fill: rec.fills[repIdx],
          stops,
        });
      }
      if (segments.length > 0) textDynamics.push({ el, segments });
    }
  }
  for (const [el, samples] of series) {
    if (seen.has(el)) continue;
    const varying = pagxWhichVary(samples);
    if (varying.length === 0) continue;
    let norm: PagxAnimStop[] = [];
    for (let i = 0; i < sampleCount; i++) {
      const offset = i / (sampleCount - 1);
      const props: Record<string, string> = {};
      for (const pr of varying) {
        const v = samples[i][pr];
        if (v != null) {
          props[pr] = v;
        } else if (pr === 'transform') {
          // `pagxExtractTranslate` returns null for an identity / `none`
          // transform, so a base-positioned sample would otherwise omit the
          // channel entirely. When `transform` is a varying channel (e.g. an
          // element that sits at translate(0) until a class toggle slides it
          // out), the missing stops let the browser interpolate `transform`
          // from the element's base value across the whole gap up to the first
          // explicit keyframe — the element drifts from t=0 instead of holding
          // still until the change. Pin the identity value explicitly so the
          // keyframe holds the base position until the real transition point.
          props[pr] = 'translate(0px, 0px)';
        }
      }
      if (Object.keys(props).length > 0) norm.push({ offset, props });
    }
    if (norm.length < 2) continue;
    // Dense linear samples → minimal keyframe set via per-channel RDP. Replaces
    // the old flat-run-only reducer: RDP additionally collapses collinear
    // (constant-velocity) runs, so a denser sample grid does not bloat the file.
    norm = pagxDecimateStops(norm);
    captured.push({
      el,
      keyframes: norm,
      durationMs,
      delayMs,
      iterations,
      direction: direction || 'normal',
      timing: 'linear',
    });
    seen.add(el);
  }
}

// ===== Shared "clock": global-duration measurement + deterministic seek =====
//
// These three browser-side functions are the shared timeline that both the
// animation *capture* (the universal sampler below, bundled into the in-page
// IIFE) and the animation *baseline* (eval-animation/baseline-frames.js, which
// injects them via `page.evaluate(fn)`) run against. Keeping one copy is what
// lets the two share a clock: the capture samples each animation at the same
// absolute playback times the baseline seeks to. They MUST be self-contained
// (only browser globals + the `window.__pagxBaselineAnime` handoff) so they
// survive both `.toString()` bundling and standalone `page.evaluate(fn)`.

// Longest single-iteration period (delay + duration) in ms across every
// animation the page reports. WAAPI (CSS animations, Motion One,
// web-animations-js) surface via `document.getAnimations()`; GSAP and anime.js
// drive their tweens off their own clocks and never appear there, so they are
// reconstructed from `gsap.globalTimeline` children / `anime.running`. One
// period is enough: looping animations realign every period, so sampling
// [0, period] covers a full visual cycle and matches how `pagx render --time`
// loops the imported timelines.
export function pagxMeasureGlobalDurationMs(): number {
  let maxMs = 0;

  let anims = [];
  try {
    anims = document.getAnimations ? document.getAnimations() : [];
  } catch (_) {
    anims = [];
  }
  for (const a of anims) {
    let ct = null;
    try {
      ct = a.effect && a.effect.getComputedTiming ? a.effect.getComputedTiming() : null;
    } catch (_) {
      ct = null;
    }
    if (!ct) continue;
    const dur = typeof ct.duration === 'number' && isFinite(ct.duration) ? ct.duration : 0;
    if (dur <= 0) continue;
    const delay = typeof ct.delay === 'number' && isFinite(ct.delay) ? ct.delay : 0;
    const end = delay + dur;
    if (end > maxMs) maxMs = end;
  }

  // GSAP: reconstruct one forward-iteration period from the global timeline's
  // top-level children. `globalTimeline.duration()` is unusable when any tween
  // repeats forever (GSAP reports ~1e10 s), which would scatter samples across
  // random loop phases. Each child exposes its single-iteration `duration()` /
  // `startTime()`, from which the true loop period is derived.
  try {
    const gsap = window.gsap;
    if (gsap && gsap.globalTimeline) {
      const tl = gsap.globalTimeline;
      let children = [];
      try {
        children = typeof tl.getChildren === 'function' ? tl.getChildren(false, true, true) : [];
      } catch (_) {
        children = [];
      }
      let windowSec = 0;
      for (const c of children) {
        let start = 0;
        let iterDur = 0;
        try { start = typeof c.startTime === 'function' ? c.startTime() : 0; } catch (_) { start = 0; }
        try { iterDur = typeof c.duration === 'function' ? c.duration() : 0; } catch (_) { iterDur = 0; }
        if (!isFinite(iterDur) || iterDur <= 0) continue;
        const end = (isFinite(start) ? start : 0) + iterDur;
        if (end > windowSec) windowSec = end;
      }
      if (windowSec > 0 && windowSec * 1000 > maxMs) {
        maxMs = windowSec * 1000;
      }
    }
  } catch (_) {
    /* ignore */
  }

  // anime.js: longest running instance.
  try {
    const anime = window.anime;
    if (anime && anime.running) {
      for (const inst of anime.running) {
        const dur = inst && typeof inst.duration === 'number' ? inst.duration : 0;
        if (isFinite(dur) && dur > maxMs) maxMs = dur;
      }
    }
  } catch (_) {
    /* ignore */
  }

  return maxMs;
}

// Snapshot the rAF-driven anime.js instances *before* any seek pauses them.
// anime.js removes paused instances from `anime.running` on its next tick, so
// once a seek pauses the first instance, `anime.running` empties and every
// later seek finds nothing. We keep our own references on `window` and seek
// those instead.
export function pagxCaptureAnimeInstances(): void {
  try {
    const anime = window.anime;
    window.__pagxBaselineAnime = (anime && anime.running) ? anime.running.slice() : [];
  } catch (_) {
    window.__pagxBaselineAnime = [];
  }
}

// Pause every animation and pin its playback time to `timeMs`. Setting
// `currentTime` on a WAAPI animation honours its own delay / iteration /
// direction, so a single absolute time seeks all animations to a consistent
// frame regardless of their individual periods. GSAP / anime.js tweens are
// seeked on their own clocks so the captured keyframes line up with the ground
// truth. Call `pagxCaptureAnimeInstances()` once before the first seek.
export function pagxSeekAllToTime(timeMs: number): void {
  const t = timeMs;
  // Drive the virtual clock (PAGX_VIRTUAL_CLOCK_INIT_SCRIPT) to this absolute
  // time FIRST, so any setTimeout/setInterval-driven scene changes (class
  // toggles, scripted `.click()`s) have mutated the DOM before we seek the
  // declarative animations and read computed style. Forward-only; no-op when
  // the clock init script was not installed.
  try {
    const clock = (window as unknown as { __pagxClock?: { advanceTo?: (ms: number) => void } }).__pagxClock;
    if (clock && typeof clock.advanceTo === 'function') clock.advanceTo(t);
  } catch (_) {
    /* ignore */
  }
  let anims = [];
  try {
    anims = document.getAnimations ? document.getAnimations() : [];
  } catch (_) {
    anims = [];
  }
  for (const a of anims) {
    try {
      a.pause();
      // Seek relative to the animation's own birth time on the virtual clock.
      // An animation attached at load is born at 0, so this is the absolute `t`
      // (its own delay / iteration / direction still resolve from `currentTime`).
      // A short animation created mid-timeline by a class toggle / scripted
      // click (e.g. a 0.42s `perkPulse` born at ~6.1s) would, under a plain
      // `currentTime = t`, be seeked far past its end and freeze on its final
      // frame — erasing the effect from BOTH the baseline and the capture.
      // Subtracting its birth plays it at the correct local phase during its
      // brief lifetime, so click "pop" / flash effects survive. `__pagxBirth`
      // is tagged deterministically by the virtual clock (PAGX_VIRTUAL_CLOCK_INIT_SCRIPT),
      // so the baseline and the capture record identical births and stay aligned.
      let birth = 0;
      try {
        const b = (a as unknown as { __pagxBirth?: number }).__pagxBirth;
        if (typeof b === 'number' && isFinite(b)) birth = b;
      } catch (_) {
        birth = 0;
      }
      const local = t - birth;
      a.currentTime = local > 0 ? local : 0;
    } catch (_) {
      /* skip this animation */
    }
  }

  // GSAP: seek the global timeline to the same absolute time. `suppressEvents =
  // true` renders the exact frame state without firing timeline events (the
  // event-firing path renders lazily and drops zero-duration `.set()` values
  // pinned to a boundary).
  try {
    const gsap = window.gsap;
    if (gsap && gsap.globalTimeline) {
      const tl = gsap.globalTimeline;
      tl.pause();
      tl.time(t / 1000, true);
    }
  } catch (_) {
    /* ignore */
  }

  // anime.js: seek each instance captured up front by pagxCaptureAnimeInstances.
  try {
    const captured = window.__pagxBaselineAnime;
    const instances = (Array.isArray(captured) && captured.length)
      ? captured
      : (window.anime && window.anime.running ? window.anime.running : []);
    for (const inst of instances) {
      try {
        if (inst.pause) inst.pause();
        const dur = typeof inst.duration === 'number' ? inst.duration : t;
        if (!isFinite(dur) || dur <= 0) continue;
        // Match the capture layer's iteration math: anime.js loop is tri-state
        // (true | number | falsy) and direction may be 'alternate' or 'reverse'.
        let iterations = 1;
        if (inst.loop === true) iterations = Infinity;
        else if (typeof inst.loop === 'number' && inst.loop > 0) iterations = inst.loop;
        const totalDur = isFinite(iterations) ? dur * iterations : Infinity;
        let local = t;
        if (isFinite(totalDur) && local >= totalDur) {
          local = totalDur;
        }
        let iterIndex = Math.floor(local / dur);
        if (!isFinite(iterations) || iterIndex >= iterations) {
          iterIndex = isFinite(iterations) ? Math.max(0, iterations - 1) : 0;
        }
        let phase = local - iterIndex * dur;
        if (phase > dur) phase = dur;
        if (inst.direction === 'reverse') {
          phase = dur - phase;
        } else if (inst.direction === 'alternate' && (iterIndex % 2) === 1) {
          phase = dur - phase;
        }
        inst.seek(Math.max(0, Math.min(phase, dur)));
      } catch (_) {
        /* skip instance */
      }
    }
  } catch (_) {
    /* ignore */
  }

  try { void document.body.offsetHeight; } catch (_) { /* ignore */ }
}

// Even sample grid over [0, globalMs]: N points including both endpoints. Pure
// (no DOM), used Node-side by baseline-frames.js so the PAGX render can be
// driven at the identical times.
export function pagxSampleTimesMs(globalMs: number, samples: number): number[] {
  if (!(globalMs > 0) || samples <= 1) return [0];
  const out: number[] = [];
  for (let i = 0; i < samples; i++) {
    out.push((i / (samples - 1)) * globalMs);
  }
  return out;
}

// Choose how many evenly-spaced samples the global sampler takes across a
// timeline of `globalMs`. Because the sampler drives the whole page on one
// clock, its temporal resolution is `globalMs / (count - 1)`: a *fixed* count
// (fine for a short 1–3s loop) becomes far too coarse on a long timer-driven
// demo. A 28.9s auto-play timeline at the base 24 samples resolves only ~1.25s,
// which collapses fast staggered entrances (e.g. 20 skill perks fading in 45ms
// apart) and brief click pulses into a single indistinguishable step — every
// perk then shares one set of keyframes and pops in together instead of in a
// wave. Scaling the count keeps the step near `stepMs` so those short motions
// survive, floored at `baseCount` (short loops keep their proven density) and
// capped at `maxCount` so a pathologically long timeline cannot blow up the
// O(count × elements) probe. RDP decimation (`pagxDecimateStops`) still
// collapses the denser grid back to the few keyframes each channel needs, so
// the extra samples buy temporal fidelity without bloating the output.
export function pagxGlobalSampleCount(
  globalMs: number,
  baseCount: number,
  stepMs = 40,
  maxCount = 900,
): number {
  if (!(globalMs > 0) || !isFinite(globalMs)) return baseCount;
  const want = Math.ceil(globalMs / stepMs) + 1;
  return Math.max(baseCount, Math.min(maxCount, want));
}

// Universal capture: sample the *whole page* on one shared global clock instead
// of walking each animation source separately. This mirrors exactly what
// eval-animation/baseline-frames.js does for the pixel ground truth — measure
// the global timeline length (`pagxMeasureGlobalDurationMs`), then seek every
// animation (WAAPI / CSS / GSAP / anime) to N evenly-spaced absolute times
// (`pagxSeekAllToTime`) and read `getComputedStyle`. Any element whose tracked
// channels (opacity / translate / color / background-color) vary across the
// window becomes one `@keyframes` spanning the full global window
// (`duration = globalMs`, `iterations = 1`, `direction = normal`, linear
// timing with the keyframe stops carrying the curve), so the imported PAGX
// timeline and the baseline share one clock and line up frame-for-frame.
//
// Requires the page to have been loaded with PAGX_ANIM_PAUSE_INIT_SCRIPT so
// finite animations are still seekable at capture time (they would otherwise
// have finished during settle and dropped out of `document.getAnimations()`).
// The whole sampling loop runs synchronously inside a single `page.evaluate`,
// so rAF-driven library tweens only advance when we explicitly seek them.
export function pagxCollectGlobalSampled(
  captured: unknown[],
  seen: Set<Element>,
  sampleCount: number,
  maxElements: number,
  textDynamics?: PagxTextDynamic[],
): void {
  let globalMs = 0;
  try {
    globalMs = pagxMeasureGlobalDurationMs();
  } catch (_) {
    globalMs = 0;
  }
  if (!(globalMs > 0) || !isFinite(globalMs)) return;
  try {
    pagxCaptureAnimeInstances();
  } catch (_) {
    /* ignore */
  }
  // Suppress CSS transitions for the duration of the sampling seeks. When the
  // virtual clock (pagxSeekAllToTime → advanceTo) fires a timer that toggles a
  // scene class, the resulting CSS *transition* would need real wall-clock time
  // (or a compositor tick) to interpolate — but the whole sampler runs in one
  // synchronous `page.evaluate`, so a transitioned property would read its
  // *start* value at every sample and the change would be invisible (e.g. a
  // panel revealed by `transition: opacity .55s` on a `.show` class would never
  // appear to fade in, and its element would be dropped as opacity:0). Forcing
  // `transition: none` makes each class flip apply its *end* value instantly at
  // the exact virtual time it happens, so the sampler captures it as a step on
  // the timeline. This is safe for load-time entrance transitions: their class
  // flip already happened before t=0, so with transitions off they read their
  // settled end value with zero variance across the window and are skipped here
  // — leaving them to the dedicated transitionrun recorder (pagxCollectTransitions),
  // which reconstructs their real tween.
  let txBlocker: HTMLStyleElement | null = null;
  try {
    txBlocker = document.createElement('style');
    txBlocker.textContent = '*, *::before, *::after { transition: none !important; }';
    (document.head || document.documentElement).appendChild(txBlocker);
  } catch (_) {
    txBlocker = null;
  }
  // Reuse the dense-sample -> per-channel RDP path. The seek closure maps the
  // 0..1 progress the sampler walks onto the shared absolute clock. Emit as a
  // single non-looping window (iterations = 1, delay = 0, normal direction):
  // any per-animation loop within [0, globalMs] is baked into the stops, which
  // is what keeps playback aligned with the baseline's per-time seek. The
  // sample count scales with `globalMs` so a long timeline keeps a fine enough
  // temporal step to preserve short staggered entrances / brief pulses (see
  // pagxGlobalSampleCount); short loops fall back to the base density.
  const effectiveSamples = pagxGlobalSampleCount(globalMs, sampleCount);
  pagxSampleTimeline(
    captured, seen, globalMs, (p) => pagxSeekAllToTime(p * globalMs),
    1, effectiveSamples, maxElements, 'normal', 0, textDynamics,
  );
  try {
    if (txBlocker && txBlocker.parentNode) txBlocker.parentNode.removeChild(txBlocker);
  } catch (_) {
    /* ignore */
  }
  try {
    pagxSeekAllToTime(0);
  } catch (_) {
    /* ignore */
  }
}

// Materialise the dynamic-text elements the global sampler found into overlay
// text layers. Runs *after* pagxDomRestore (so the created nodes survive into
// the snapshot — restore drops anything appended during sampling) and *before*
// pagxEmitCaptured, pushing one opacity-animated descriptor per segment so the
// shared emit path installs its `@keyframes` uniformly.
//
// For each element (e.g. `#energyCombo`, whose text the demo scripts through
// `+1 … +20`) the base node's own text is cleared — its box / bg keeps whatever
// opacity/transform the sampler already captured — and each distinct value it
// showed becomes a shallow clone stacked right after it, carrying that value's
// glyph, the size / color it wore while visible, and an opacity window that
// fades it in and out at the exact global times the baseline shows it. Clones
// keep the element's class + base inline style (so position, font family, glow
// and transform-origin all match) but drop its `id` to avoid duplicate ids in
// the serialised DOM. This targets bare text leaves; a leaf that also painted a
// background would see that bg duplicated on each overlay, but scripted
// counters / countdowns are glyph-only in practice.
export function pagxBuildTextOverlays(
  textDynamics: PagxTextDynamic[],
  captured: unknown[],
  globalMs: number,
): void {
  if (!Array.isArray(textDynamics) || textDynamics.length === 0) return;
  for (const dyn of textDynamics) {
    const el = dyn.el;
    if (!el || (el as { isConnected?: boolean }).isConnected === false) continue;
    if (!el.parentNode) continue;
    // Clear the base element's own text nodes so the animated box no longer
    // carries a stale glyph; the overlays below own the changing text.
    try {
      for (const node of Array.prototype.slice.call(el.childNodes)) {
        if (node.nodeType === 3 && node.parentNode) node.parentNode.removeChild(node);
      }
    } catch (_) {
      /* ignore */
    }
    for (const seg of dyn.segments) {
      let clone: HTMLElement;
      try {
        clone = el.cloneNode(false) as HTMLElement;
      } catch (_) {
        continue;
      }
      try {
        clone.removeAttribute('id');
        clone.textContent = seg.text;
        if (seg.fontSize) clone.style.fontSize = seg.fontSize;
        if (seg.color) clone.style.color = seg.color;
        // SVG `<text>` paints with `fill`, not `color`, and dropping the source
        // `id` above severs any `#id { fill }` author rule — so pin the sampled
        // fill directly. Harmless on HTML text (which ignores `fill`). Skip
        // `none` / gradient-reference paints, which are not plain colors.
        if (seg.fill && seg.fill !== 'none' && seg.fill.indexOf('url(') === -1) {
          try { clone.style.setProperty('fill', seg.fill); } catch (_) { /* ignore */ }
        }
        el.parentNode.insertBefore(clone, el.nextSibling);
      } catch (_) {
        continue;
      }
      captured.push({
        el: clone,
        keyframes: seg.stops,
        durationMs: globalMs,
        delayMs: 0,
        iterations: 1,
        direction: 'normal',
        timing: 'linear',
      });
    }
  }
}

// Compute the sampling window for GSAP's global timeline: one *forward iteration*
// of the longest top-level child, plus whether the composition repeats forever
// and whether any repeating child uses `yoyo`. Also returns the maximum finite
// repeat count seen across children so finite repeats survive into the captured
// `iterations` instead of being collapsed.
//
// `gsap.globalTimeline.duration()` is unusable here: when any tween repeats
// forever (`repeat: -1`) GSAP reports the timeline duration as ~1e10 s, so
// sampling evenly across it lands on random phases of the real loop (producing
// keyframes that jump back and forth instead of progressing). Each child tween /
// nested timeline instead exposes its single-iteration `duration()`, `startTime()`,
// `repeat()` and `yoyo()`, from which the true loop period is reconstructed.
//
// Children whose `startTime()` is non-zero (staggered entrances) are folded into
// the window via `windowSec = max(start + iterDur)`, so the leading hold while
// they wait is captured as flat keyframes inside the canonical `@keyframes`. The
// shorthand's `animation-delay` stays at `0s`; otherwise the importer would
// offset playback by that delay a second time on top of the already-baked hold.
//
// A `yoyo` repeat maps to CSS `animation-direction: alternate`, so only the
// forward half is sampled and the runtime mirrors it back — matching GSAP's
// forward-then-reverse playback.
export function pagxGsapWindow(gsap: { globalTimeline: unknown }): {
  windowSec: number;
  infinite: boolean;
  yoyo: boolean;
  iterations: number;
} {
  const tl = gsap.globalTimeline as {
    getChildren?: (nested: boolean, tweens: boolean, timelines: boolean) => unknown[];
  };
  let children: unknown[] = [];
  try {
    children = typeof tl.getChildren === 'function' ? tl.getChildren(false, true, true) : [];
  } catch (_) {
    children = [];
  }
  let windowSec = 0;
  let infinite = false;
  let yoyo = false;
  let iterations = 1;
  for (const c of children) {
    const child = c as {
      startTime?: () => number;
      duration?: () => number;
      repeat?: () => number;
      yoyo?: () => boolean;
    };
    let start = 0;
    let iterDur = 0;
    let rep = 0;
    let cyoyo = false;
    try {
      start = typeof child.startTime === 'function' ? child.startTime() : 0;
    } catch (_) {
      start = 0;
    }
    try {
      iterDur = typeof child.duration === 'function' ? child.duration() : 0;
    } catch (_) {
      iterDur = 0;
    }
    try {
      rep = typeof child.repeat === 'function' ? child.repeat() || 0 : 0;
    } catch (_) {
      rep = 0;
    }
    try {
      cyoyo = typeof child.yoyo === 'function' ? !!child.yoyo() : false;
    } catch (_) {
      cyoyo = false;
    }
    if (!isFinite(iterDur) || iterDur <= 0) continue;
    if (rep < 0) {
      infinite = true;
    } else if (rep + 1 > iterations) {
      // GSAP's `repeat()` is the number of *additional* plays after the first; the user-visible
      // iteration count is rep + 1.
      iterations = rep + 1;
    }
    if (rep !== 0 && cyoyo) yoyo = true;
    const startFinite = isFinite(start) ? start : 0;
    const end = startFinite + iterDur;
    if (end > windowSec) windowSec = end;
  }
  return { windowSec, infinite, yoyo, iterations };
}

export function pagxCollectGsap(
  captured: unknown[],
  seen: Set<Element>,
  sampleCount: number,
  maxElements: number,
): void {
  const gsap = (window as { gsap?: unknown }).gsap;
  if (!gsap || !gsap.globalTimeline) return;
  const tl = gsap.globalTimeline;
  const win = pagxGsapWindow(gsap);
  if (!win.windowSec || !isFinite(win.windowSec) || win.windowSec <= 0) return;
  try {
    tl.pause();
  } catch (_) {
    /* ignore */
  }
  const direction = win.yoyo ? 'alternate' : 'normal';
  // `iterations` matches CSS animation-iteration-count semantics: Infinity for repeat: -1,
  // otherwise the maximum repeat+1 across children. yoyo's forward-half-only sampling pairs
  // with `direction: alternate` so the runtime mirrors the captured forward window.
  const iterations = win.infinite ? Infinity : win.iterations;
  // Seek with `suppressEvents = true`. GSAP's event-firing seek path (the
  // `false` default) renders lazily and mishandles a zero-duration `.set()`
  // pinned to a timeline boundary (e.g. a wrapper's `autoAlpha: 1` placed at
  // position 0): the value never re-applies when the playhead jumps onto that
  // boundary, so the element samples as its pre-set state (opacity 0) for the
  // whole window and its animated channel is lost. Suppressing events forces a
  // full state render at each sample time — exactly what a static frame capture
  // wants — and matches the seek baseline-frames.js uses for the ground truth.
  // delayMs is forced to 0: the sampling window already spans [0, windowSec],
  // so any pre-animation hold (a child whose startTime > 0) is encoded directly
  // as leading flat keyframes inside the canonical `@keyframes`. Forwarding
  // `win.delaySec` here would write the same delay a second time into the
  // `animation-delay` part of the shorthand, doubling the offset and pushing
  // the visible motion past the loop window when the importer plays the
  // animation back.
  pagxSampleTimeline(
    captured, seen, win.windowSec * 1000, (p) => tl.time(p * win.windowSec, true),
    iterations, sampleCount, maxElements, direction, 0,
  );
  try {
    tl.time(0, true);
    tl.pause();
  } catch (_) {
    /* ignore */
  }
}

export function pagxCollectAnime(
  captured: unknown[],
  seen: Set<Element>,
  sampleCount: number,
  maxElements: number,
): void {
  const anime = (window as { anime?: unknown }).anime;
  if (!anime || !anime.running) return;
  let running: unknown[] = [];
  try {
    running = anime.running.slice();
  } catch (_) {
    return;
  }
  for (const inst of running) {
    try {
      const dur = inst.duration;
      if (!dur || dur <= 0) continue;
      if (inst.pause) inst.pause();
      // anime.js `loop` is tri-state: true = infinite, a positive number = explicit count, any
      // other falsy value = single iteration. `direction` mirrors CSS animation-direction.
      let iterations = 1;
      if (inst.loop === true) {
        iterations = Infinity;
      } else if (typeof inst.loop === 'number' && inst.loop > 0) {
        iterations = inst.loop;
      }
      const direction = (inst.direction === 'alternate' || inst.direction === 'reverse')
        ? inst.direction
        : 'normal';
      // delayMs is forced to 0: anime.js's `inst.duration` includes the
      // configured `delay`, so `inst.seek(p * dur)` samples the entire
      // [0, dur] window — including the pre-delay hold where the element
      // sits at its starting state. That hold is already captured as the
      // leading flat keyframes in `@keyframes`, so writing `inst.delay`
      // into the shorthand's `animation-delay` would offset playback by
      // delay a second time, masking the visible motion.
      pagxSampleTimeline(
        captured, seen, dur, (p) => inst.seek(p * dur), iterations, sampleCount,
        maxElements, direction, 0,
      );
      inst.seek(0);
    } catch (_) {
      /* skip instance */
    }
  }
}

// Install the early `transitionrun` recorder on `window.__pagxTransitions`.
//
// CSS transitions are invisible to the late capture pass: a transition only
// surfaces in `document.getAnimations()` while it is actively interpolating,
// and by snapshot time every load-triggered entrance transition (the dominant
// real-world use: an element starts at `opacity:0`/`translateY(20px)` and a
// class flip on `rAF`/`load` transitions it to its resting state) has already
// finished — leaving only the final value, with the start value lost.
//
// This runs at document-start (registered as an init script *before* the page's
// own scripts) and listens for `transitionrun`, which the engine fires the
// instant a transition is created — before its delay and before any visible
// progress. At that moment `getComputedStyle` still reports the *start* value,
// so we record it per (element, property) along with the property's resolved
// duration / delay / timing-function. `pagxCollectTransitions` later pairs each
// recorded start with the element's settled end value to synthesise the
// equivalent `@keyframes`.
//
// Only the runtime-playable channels are tracked (opacity / transform / color /
// background-color); `transitionrun` always reports a concrete longhand in
// `propertyName` even when the author wrote `transition: all`, so the filter is
// exact. Last write wins per (element, property): a re-triggered transition
// (e.g. the same element animated twice) keeps the most recent start, which is
// the one whose end value we read at capture time.
export function pagxTransitionInstall(): void {
  try {
    const w = window as unknown as { __pagxTransitions?: unknown };
    if (w.__pagxTransitions) return;
    const store: { map: Map<Element, Record<string, unknown>> } = { map: new Map() };
    w.__pagxTransitions = store;
    const tracked: Record<string, true> = {
      opacity: true,
      transform: true,
      color: true,
      'background-color': true,
    };
    const record = (e: TransitionEvent): void => {
      try {
        const el = e.target as Element;
        if (!el || el.nodeType !== 1) return;
        const prop = e.propertyName;
        if (!prop || !tracked[prop]) return;
        const cs = getComputedStyle(el as HTMLElement);
        const props = pagxSplitTopLevelCommas(cs.transitionProperty || '');
        const durs = pagxSplitTopLevelCommas(cs.transitionDuration || '');
        const dels = pagxSplitTopLevelCommas(cs.transitionDelay || '');
        const tims = pagxSplitTopLevelCommas(cs.transitionTimingFunction || '');
        let idx = props.indexOf(prop);
        if (idx < 0) idx = props.indexOf('all');
        if (idx < 0) idx = 0;
        const durationMs = pagxParseTimeMs(durs[idx] || durs[0] || '0s');
        const delayMs = pagxParseTimeMs(dels[idx] || dels[0] || '0s');
        const timing = (tims[idx] || tims[0] || 'linear').trim();
        const from = prop === 'transform'
          ? cs.transform
          : cs.getPropertyValue(prop);
        let perEl = store.map.get(el);
        if (!perEl) {
          perEl = {};
          store.map.set(el, perEl);
        }
        perEl[prop] = { from, durationMs, delayMs, timing };
      } catch (_) {
        /* ignore one event */
      }
    };
    document.addEventListener('transitionrun', record, true);
  } catch (_) {
    /* transitions simply won't be captured */
  }
}

// Capture-time collector: turn every transition recorded by
// `pagxTransitionInstall` into a canonical descriptor. Reads each element's
// settled (resting) value as the transition's end state, pairs it with the
// recorded start, and merges all of the element's transitioned channels into a
// single descriptor — the importer plays one `animation` shorthand per element,
// so per-property durations are collapsed onto the longest one (its delay /
// timing-function come along). Runs last in the chain so an element already
// handled by WAAPI / `@keyframes` / GSAP / anime.js (`seen`) keeps that
// higher-fidelity capture.
export function pagxCollectTransitions(captured: unknown[], seen: Set<Element>): void {
  const store = (window as unknown as {
    __pagxTransitions?: { map?: Map<Element, Record<string, unknown>> };
  }).__pagxTransitions;
  if (!store || !store.map || typeof store.map.forEach !== 'function') return;
  store.map.forEach((perEl, el) => {
    try {
      if (!el || el.nodeType !== 1 || seen.has(el)) return;
      if ((el as { isConnected?: boolean }).isConnected === false) return;
      const cs = getComputedStyle(el as HTMLElement);
      const rect = (el as HTMLElement).getBoundingClientRect();
      const box = { width: rect.width, height: rect.height };
      const fromBag: Record<string, unknown> = {};
      const toBag: Record<string, unknown> = {};
      let bestDur = 0;
      let bestDelay = 0;
      let bestTiming = 'linear';
      for (const prop of Object.keys(perEl)) {
        const info = perEl[prop] as {
          from: string; durationMs: number; delayMs: number; timing: string;
        };
        const toVal = prop === 'transform' ? cs.transform : cs.getPropertyValue(prop);
        fromBag[prop] = info.from;
        toBag[prop] = toVal;
        if (info.durationMs > bestDur) {
          bestDur = info.durationMs;
          bestDelay = info.delayMs;
          bestTiming = info.timing;
        }
      }
      const desc = pagxTransitionDescriptorFromBags(
        fromBag, toBag, box, bestDur, bestDelay, bestTiming,
      );
      if (!desc) return;
      captured.push({ el: el as HTMLElement, ...desc });
      seen.add(el);
    } catch (_) {
      /* skip this element */
    }
  });
}

// ===== In-browser emitter =====
//
// Turn a collected `captured[]` (each `{ el } & descriptor`) into the canonical
// form the importer plays back: a `@keyframes <prefix><N>` rule injected into
// `<style id=styleId>`, plus an inline `animation` shorthand on each element.
// Driven by the instant orchestrator (`pagxAnimMain`).
export function pagxEmitCaptured(
  captured: Array<{ el: HTMLElement } & PagxAnimDescriptor>,
  prefix: string,
  styleId: string,
): { count: number; names: string[] } {
  let css = '';
  let index = 0;
  const names: string[] = [];
  for (const cap of captured) {
    // Skip elements that left the tree before emit: there is no host to attach
    // to, and the snapshot walker would not see it anyway.
    if (!cap.el || (cap.el as { isConnected?: boolean }).isConnected === false) continue;
    const built = pagxBuildCanonicalAnimation(cap, index, prefix);
    if (!built) continue;
    index++;
    css += built.keyframesCss + '\n';
    // Pin the element's static base transform onto its inline slot before
    // installing the canonical pagxAnim* shorthand. The runtime CAN play a full
    // 2D affine base (scale/rotate/skew/translate) via the importer's
    // `Layer.matrix` channel, so `pagxExtractTransform` preserves the whole
    // matrix (collapsing to `translate(…)` only when the linear part is
    // identity). This matters for two idioms that would otherwise regress:
    //   - `.loader-bar { transform: scaleX(0) }` paired with a
    //     `scaleX(0) -> scaleX(1)` keyframe animation — the keyframes drive
    //     `transform`, so we pin `none` (see the exception below) and the
    //     scaleX(0) base is neutralised.
    //   - `.wslot { transform: skewX(-12deg) }` paired with an opacity/clip-path
    //     reveal animation — the keyframes do NOT drive `transform`, so the
    //     static skew must survive; pinning the full affine keeps the box a
    //     parallelogram (and its reverse-skewed children upright) instead of
    //     flattening to an axis-aligned rectangle. A translate-only pin
    //     (the previous behaviour) silently dropped the skew/scale/rotate here.
    // Reading the base under `animation: none !important` blocks both the
    // original CSS animation and the just-installed inline pagxAnim* from
    // contributing, so the resolved transform reflects only the static base.
    // Inline-author transforms (set via `el.style.transform` directly) win over
    // class rules, so this override doesn't lose any user intent.
    //
    // Critical exception: when the captured keyframes *themselves* drive a
    // `transform` channel, that channel is sampled from computed style, which
    // already folds the element's static base transform into every stop (e.g. a
    // `.namebar { transform: translate(-88px,-50px) }` whose exit animation
    // toggles to `translateX(-560px)` samples as `translate(-88,-50)` at rest).
    // Pinning that same base inline would then double-apply it: the PAGX
    // importer writes the inline base into `Layer.matrix` AND plays the keyframe
    // transform on top, so the element lands an extra (base) offset / scale away
    // from where the browser paints it. In that case pin `none` and let the
    // keyframes be the single source of the transform channel. The full base
    // transform is only pinned when the keyframes carry no transform channel.
    const keyframesDriveTransform = cap.keyframes.some(
      (s) => s.props && s.props['transform'] != null,
    );
    const elRect = cap.el.getBoundingClientRect();
    const elBox = { width: elRect.width, height: elRect.height };
    const blocker = document.createElement('style');
    blocker.textContent = '*, *::before, *::after { animation: none !important; transition: none !important; }';
    const blockerParent = document.head || document.documentElement;
    if (blockerParent) blockerParent.appendChild(blocker);
    void document.body.offsetHeight;
    const baseT = pagxExtractTransform(getComputedStyle(cap.el).transform || '', elBox);
    if (blocker.parentNode) blocker.parentNode.removeChild(blocker);
    cap.el.style.transform = keyframesDriveTransform ? 'none' : (baseT || 'none');
    cap.el.style.animation = built.animationShorthand;
    names.push(built.name);
  }

  if (css) {
    let styleEl = document.getElementById(styleId);
    if (!styleEl) {
      styleEl = document.createElement('style');
      styleEl.id = styleId;
      document.head.appendChild(styleEl);
    }
    styleEl.textContent = css;
  }
  return { count: names.length, names };
}

// ===== In-browser orchestrator (instant) =====
//
// Runs the collector priority chain at the current instant, then emits every
// captured animation into the canonical form.
export function pagxAnimMain(opts: {
  sampleCount: number;
  maxElements: number;
  prefix: string;
  styleId: string;
}): { count: number; names: string[] } {
  const sampleCount = Math.max(2, opts.sampleCount || 24);
  const maxElements = opts.maxElements || 4000;
  const prefix = opts.prefix || 'pagxAnim';
  const styleId = opts.styleId || '__pagx_anim_keyframes';

  const captured: Array<{ el: HTMLElement } & PagxAnimDescriptor> = [];
  const seen = new Set<Element>();
  // Flush zero-delay init timers (e.g. a page that defers layout setup via
  // setTimeout(0)) onto the virtual clock so the checkpoint below captures the
  // page's true t=0 state. No-op when the clock init script was not installed.
  try {
    const clock = (window as unknown as { __pagxClock?: { advanceTo?: (ms: number) => void } }).__pagxClock;
    if (clock && typeof clock.advanceTo === 'function') clock.advanceTo(0);
  } catch (_) {
    /* ignore */
  }
  // Checkpoint the initial DOM before the sampler advances the virtual clock:
  // timer-driven scene changes will mutate the DOM as the timeline plays, and
  // the snapshot that follows must serialise the initial scene, not the end
  // state. Restored right after sampling (before emit).
  let checkpoint: ReturnType<typeof pagxDomCheckpoint> | null = null;
  try {
    checkpoint = pagxDomCheckpoint();
  } catch (_) {
    checkpoint = null;
  }
  // Universal path: one shared-clock global sampler replaces the per-source
  // collectors (WAAPI / CSS / GSAP / anime). Seeking the whole page to N
  // absolute times and reading computed style captures motion from every
  // source uniformly and on the same clock the baseline seeks to, so the
  // imported timeline aligns frame-for-frame with the ground truth. Each seek
  // also advances the virtual clock, so JS timer-driven scene changes are
  // sampled as opacity / transform curves too.
  // Elements whose `textContent` the timeline scripts through several values
  // (a combo counter, a countdown). The sampler records them here; after the
  // DOM is restored they are rebuilt as overlay text layers so the changing
  // glyph survives into the snapshot (a single static text node cannot show
  // `+1` then `+4` then `+20` at different times).
  const textDynamics: PagxTextDynamic[] = [];
  pagxCollectGlobalSampled(captured, seen, sampleCount, maxElements, textDynamics);
  try {
    pagxDomRestore(checkpoint);
  } catch (_) {
    /* ignore */
  }
  // Rebuild the scripted-text elements as overlays now that the DOM is back to
  // its initial scene (so the clones we append are not stripped by the restore
  // above). Recompute the global window — deterministic after the seek-to-0 the
  // sampler ends on — to time each overlay's fade against the same clock.
  if (textDynamics.length > 0) {
    let overlayMs = 0;
    try {
      overlayMs = pagxMeasureGlobalDurationMs();
    } catch (_) {
      overlayMs = 0;
    }
    if (overlayMs > 0 && isFinite(overlayMs)) {
      try {
        pagxBuildTextOverlays(textDynamics, captured, overlayMs);
      } catch (_) {
        /* ignore */
      }
    }
  }
  // Runs last, as a supplement: CSS transitions only interpolate on a property
  // change and are not seekable, so the global sampler cannot reconstruct one
  // that already finished during settle. The `transitionrun` recorder
  // (pagxTransitionInstall) captured its start value at load; pair it with the
  // settled end here for any element the global sampler did not already cover.
  pagxCollectTransitions(captured, seen);
  if (captured.length === 0) return { count: 0, names: [] };
  return pagxEmitCaptured(captured, prefix, styleId);
}

// ===== Node-side wrapper =====

// Functions concatenated into the in-page IIFE. Order is irrelevant: function
// declarations hoist within the shared scope, so each helper can reference any
// other regardless of textual position (same mechanism browser-snapshot.ts
// relies on for its HELPER_FNS bundle).
const PAGX_ANIM_FNS = [
  pagxExtractTranslate,
  pagxExtractTransform,
  pagxTransformToMatrix6,
  pagxPickProp,
  pagxNormalizeProps,
  pagxFillOffsets,
  pagxOffsetFromKeyText,
  pagxParseTimeMs,
  pagxWhichVary,
  pagxReduceKeyframes,
  pagxParseColorChannels,
  pagxParseFilterChannels,
  pagxParseTranslateXY,
  pagxClipNormalizeD,
  pagxStopScalarSeries,
  pagxRdpKeep,
  pagxDecimateStops,
  pagxNormalizeTiming,
  pagxSplitTopLevelCommas,
  pagxBoxShadowToFilter,
  pagxResolveWaapiEasing,
  pagxBuildCanonicalAnimation,
  pagxTransitionDescriptorFromBags,
  pagxTextSegments,
  pagxBuildTextOverlays,
  pagxCandidateElements,
  pagxDomCheckpoint,
  pagxDomRestore,
  pagxBuildKeyframesIndex,
  pagxCollectWAAPI,
  pagxCollectCSS,
  pagxCollectTransitions,
  pagxSampleTimeline,
  pagxGsapWindow,
  pagxCollectGsap,
  pagxCollectAnime,
  // Shared-clock helpers (from animation-clock.ts) + the universal global
  // sampler that pagxAnimMain now drives. Bundled by name so the in-page IIFE
  // can call them; they are self-contained (see animation-clock.ts).
  pagxMeasureGlobalDurationMs,
  pagxCaptureAnimeInstances,
  pagxSeekAllToTime,
  pagxGlobalSampleCount,
  pagxCollectGlobalSampled,
  pagxEmitCaptured,
  pagxAnimMain,
];

// Init-script form: install the early `transitionrun` recorder so CSS
// transitions that fire (and finish) during load are still observable at
// capture time. Registered via the page-loader's `initScripts` (before
// navigation), mirroring SNAPSHOT_INIT_SCRIPT / ICON_FONT_INIT_SCRIPT. Only the
// three helpers the recorder depends on are bundled — the recorder writes to
// `window.__pagxTransitions`, which `pagxCollectTransitions` (shipped later in
// the capture IIFE) reads back.
export const PAGX_TRANSITION_INIT_SCRIPT =
  '(function() {\n' +
  pagxSplitTopLevelCommas.toString() + '\n' +
  pagxParseTimeMs.toString() + '\n' +
  pagxTransitionInstall.toString() + '\n' +
  'pagxTransitionInstall();\n' +
  '})();';

// Build the self-contained IIFE evaluated in the page. The helper sources are
// concatenated and the entry call passes `opts` inlined as JSON.
export function buildAnimationCapturePayload(opts: {
  sampleCount: number;
  maxElements: number;
}): string {
  const src = PAGX_ANIM_FNS.map((fn) => fn.toString()).join('\n\n');
  const callArg = JSON.stringify({
    sampleCount: opts.sampleCount,
    maxElements: opts.maxElements,
    prefix: PAGX_ANIM_PREFIX,
    styleId: PAGX_ANIM_STYLE_ID,
  });
  return `(() => {\n${src}\nreturn pagxAnimMain(${callArg});\n})()`;
}

// Run the in-page capture pass against an already-loaded page. Best-effort:
// any failure degrades to "no animations captured" and the snapshot proceeds
// as a static frame. Mirrors `inlineIconFontsOnPage`.
export async function capturePagxAnimationsOnPage(
  page: Page,
  opts?: CaptureAnimationsOptions,
): Promise<CaptureAnimationsResult> {
  const options = opts || {};
  const logger = typeof options.logger === 'function' ? options.logger : () => {};
  try {
    const payload = buildAnimationCapturePayload({
      // Sample densely; per-channel RDP (pagxDecimateStops) collapses the grid
      // back down to the few keyframes each channel actually needs, so a high
      // count buys fidelity without bloating the output.
      sampleCount: options.sampleCount || 24,
      maxElements: options.maxElements || 4000,
    });
    const result = (await page.evaluate(payload)) as CaptureAnimationsResult;
    if (result && result.count > 0) {
      logger(`captured ${result.count} animation(s): ${result.names.join(', ')}`);
    }
    return result || { count: 0, names: [] };
  } catch (err) {
    logger(`animation capture failed: ${errMessage(err)}`);
    return { count: 0, names: [] };
  }
}
