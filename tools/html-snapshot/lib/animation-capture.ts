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
export function pagxExtractTranslate(transformStr: string): string | null {
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
  return 'translate(' + (tx || '0px') + ', ' + (ty || '0px') + ')';
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
// tracked, runtime-playable subset (kebab-cased keys).
export function pagxNormalizeProps(raw: Record<string, unknown>): Record<string, string> {
  const out: Record<string, string> = {};
  const op = pagxPickProp(raw, 'opacity', 'opacity');
  if (op != null) out['opacity'] = op;
  const tr = pagxPickProp(raw, 'transform', 'transform');
  if (tr != null && tr !== 'none') {
    const t = pagxExtractTranslate(tr);
    if (t) out['transform'] = t;
  }
  const col = pagxPickProp(raw, 'color', 'color');
  if (col != null) out['color'] = col;
  const bg = pagxPickProp(raw, 'background-color', 'backgroundColor');
  if (bg != null) out['background-color'] = bg;
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
export function pagxWhichVary(samples: Array<Record<string, string | null>>): string[] {
  const props = ['opacity', 'transform', 'color', 'background-color'];
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
      const t = pagxParseTranslateXY(p['transform']);
      if (t) {
        put('tx', i, t[0]);
        put('ty', i, t[1]);
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
  }
  return series;
}

// RDP on one scalar channel. `offsets` and `values` are parallel; values are
// normalised to their own [0,1] travel so `eps` is a scale-free fraction of the
// channel's total motion (a 2px wiggle and a 200px slide are simplified to the
// same relative tolerance). Returns a per-index keep mask. A flat channel
// (zero travel) keeps only its endpoints.
export function pagxRdpKeep(offsets: number[], values: number[], eps: number): boolean[] {
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
    if (maxD > eps && idx > a && idx < b) {
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
export function pagxDecimateStops(stops: PagxAnimStop[], eps = 0.01): PagxAnimStop[] {
  if (stops.length <= 2) return stops;
  const offsets = stops.map((s) => s.offset);
  const series = pagxStopScalarSeries(stops);
  const keep = new Array(stops.length).fill(false);
  keep[0] = true;
  keep[stops.length - 1] = true;
  for (const key of Object.keys(series)) {
    const vals = series[key];
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
    const k = pagxRdpKeep(offsets, vals, eps);
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
  const animationShorthand =
    name + ' ' + durSec + 's ' + pagxNormalizeTiming(cap.timing) + ' ' +
    delaySec + 's ' + iter + ' ' + dir;
  return { name, keyframesCss, animationShorthand };
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
      for (const kf of effect.getKeyframes()) {
        const props = pagxNormalizeProps(kf);
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
    for (const kf of Array.prototype.slice.call(rule.cssRules)) {
      const offsets = (kf.keyText || '').split(',');
      const props = pagxNormalizeProps({
        opacity: kf.style.opacity,
        transform: kf.style.transform,
        color: kf.style.color,
        backgroundColor: kf.style.backgroundColor,
      });
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
): void {
  const candidates = pagxCandidateElements(maxElements);
  const series = new Map<HTMLElement, Array<Record<string, string | null>>>();
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
      const snap: Record<string, string | null> = {
        opacity: cs.opacity,
        transform: pagxExtractTranslate(cs.transform),
        color: cs.color,
        'background-color': cs.backgroundColor,
      };
      let arr = series.get(el);
      if (!arr) {
        arr = [];
        series.set(el, arr);
      }
      arr.push(snap);
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
        if (v != null) props[pr] = v;
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

// Compute the sampling window for GSAP's global timeline: one *forward iteration*
// of the longest top-level child, plus whether the composition repeats forever
// and whether any repeating child uses `yoyo`. Also returns the maximum finite
// repeat count seen across children and the maximum start delay, so finite
// repeats and staggered entrances survive into the captured `iterations` /
// `delayMs` instead of being collapsed.
//
// `gsap.globalTimeline.duration()` is unusable here: when any tween repeats
// forever (`repeat: -1`) GSAP reports the timeline duration as ~1e10 s, so
// sampling evenly across it lands on random phases of the real loop (producing
// keyframes that jump back and forth instead of progressing). Each child tween /
// nested timeline instead exposes its single-iteration `duration()`, `startTime()`,
// `repeat()` and `yoyo()`, from which the true loop period is reconstructed.
//
// A `yoyo` repeat maps to CSS `animation-direction: alternate`, so only the
// forward half is sampled and the runtime mirrors it back — matching GSAP's
// forward-then-reverse playback.
export function pagxGsapWindow(gsap: { globalTimeline: unknown }): {
  windowSec: number;
  infinite: boolean;
  yoyo: boolean;
  iterations: number;
  delaySec: number;
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
  let delaySec = 0;
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
    if (startFinite > delaySec) delaySec = startFinite;
    const end = startFinite + iterDur;
    if (end > windowSec) windowSec = end;
  }
  return { windowSec, infinite, yoyo, iterations, delaySec };
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
  pagxSampleTimeline(
    captured, seen, win.windowSec * 1000, (p) => tl.time(p * win.windowSec, true),
    iterations, sampleCount, maxElements, direction, win.delaySec * 1000,
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
      const delayMs = (typeof inst.delay === 'number' && isFinite(inst.delay) && inst.delay > 0)
        ? inst.delay
        : 0;
      pagxSampleTimeline(
        captured, seen, dur, (p) => inst.seek(p * dur), iterations, sampleCount,
        maxElements, direction, delayMs,
      );
      inst.seek(0);
    } catch (_) {
      /* skip instance */
    }
  }
}

// ===== In-browser orchestrator =====
//
// Runs the collector priority chain, then rewrites every captured animation
// into the canonical form: a `@keyframes` rule in <style id> and an inline
// `animation` shorthand on the element.
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
  pagxCollectWAAPI(captured, seen);
  pagxCollectCSS(captured, seen, maxElements);
  pagxCollectGsap(captured, seen, sampleCount, maxElements);
  pagxCollectAnime(captured, seen, sampleCount, maxElements);
  if (captured.length === 0) return { count: 0, names: [] };

  let css = '';
  let index = 0;
  const names: string[] = [];
  for (const cap of captured) {
    const built = pagxBuildCanonicalAnimation(cap, index, prefix);
    if (!built) continue;
    index++;
    css += built.keyframesCss + '\n';
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

// ===== Node-side wrapper =====

// Functions concatenated into the in-page IIFE. Order is irrelevant: function
// declarations hoist within the shared scope, so each helper can reference any
// other regardless of textual position (same mechanism browser-snapshot.ts
// relies on for its HELPER_FNS bundle).
const PAGX_ANIM_FNS = [
  pagxExtractTranslate,
  pagxPickProp,
  pagxNormalizeProps,
  pagxFillOffsets,
  pagxOffsetFromKeyText,
  pagxParseTimeMs,
  pagxWhichVary,
  pagxReduceKeyframes,
  pagxParseColorChannels,
  pagxParseTranslateXY,
  pagxStopScalarSeries,
  pagxRdpKeep,
  pagxDecimateStops,
  pagxNormalizeTiming,
  pagxSplitTopLevelCommas,
  pagxResolveWaapiEasing,
  pagxBuildCanonicalAnimation,
  pagxCandidateElements,
  pagxBuildKeyframesIndex,
  pagxCollectWAAPI,
  pagxCollectCSS,
  pagxSampleTimeline,
  pagxGsapWindow,
  pagxCollectGsap,
  pagxCollectAnime,
  pagxAnimMain,
];

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
