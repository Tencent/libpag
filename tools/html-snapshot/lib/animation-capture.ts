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
  // Number of samples taken across a library timeline (GSAP / anime.js).
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
// non-translation transform component is dropped (returns null).
export function pagxExtractTranslate(transformStr: string): string | null {
  const s = (transformStr || '').trim().toLowerCase();
  if (!s || s === 'none') return null;
  let m = s.match(/^matrix\(([^)]+)\)$/);
  if (m) {
    const n = m[1].split(',').map((x) => parseFloat(x.trim()));
    if (n.length === 6 && n[0] === 1 && n[1] === 0 && n[2] === 0 && n[3] === 1) {
      return 'translate(' + n[4] + 'px, ' + n[5] + 'px)';
    }
    return null;
  }
  m = s.match(/^matrix3d\(([^)]+)\)$/);
  if (m) {
    const n = m[1].split(',').map((x) => parseFloat(x.trim()));
    const pureTranslate =
      n.length === 16 && n[0] === 1 && n[5] === 1 && n[10] === 1 && n[15] === 1 &&
      n[1] === 0 && n[2] === 0 && n[3] === 0 && n[4] === 0 && n[6] === 0 && n[7] === 0 &&
      n[8] === 0 && n[9] === 0 && n[11] === 0 && n[14] === 0;
    if (pureTranslate) return 'translate(' + n[12] + 'px, ' + n[13] + 'px)';
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

export function pagxNormalizeTiming(t: string): string {
  const v = (t || '').trim();
  if (!v) return 'linear';
  return v;
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
      let easing = 'linear';
      try {
        easing = (effect.getTiming && effect.getTiming().easing) || ct.easing || 'linear';
      } catch (_) {
        /* keep linear */
      }
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
  loopInfinite: boolean,
  sampleCount: number,
  maxElements: number,
): void {
  const candidates = pagxCandidateElements(maxElements);
  const series = new Map<HTMLElement, Array<Record<string, string | null>>>();
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
    norm = pagxReduceKeyframes(norm);
    captured.push({
      el,
      keyframes: norm,
      durationMs,
      delayMs: 0,
      iterations: loopInfinite ? Infinity : 1,
      direction: 'normal',
      timing: 'linear',
    });
    seen.add(el);
  }
}

export function pagxCollectGsap(
  captured: unknown[],
  seen: Set<Element>,
  sampleCount: number,
  maxElements: number,
): void {
  const gsap = (window as { gsap?: unknown }).gsap;
  if (!gsap || !gsap.globalTimeline) return;
  let total = 0;
  try {
    total = gsap.globalTimeline.duration();
  } catch (_) {
    return;
  }
  if (!total || !isFinite(total) || total <= 0) return;
  const tl = gsap.globalTimeline;
  pagxSampleTimeline(
    captured, seen, total * 1000, (p) => tl.progress(p, false), true, sampleCount, maxElements,
  );
  try {
    tl.progress(0, false);
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
      pagxSampleTimeline(
        captured, seen, dur, (p) => inst.seek(p * dur), inst.loop === true, sampleCount, maxElements,
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
  const sampleCount = Math.max(2, opts.sampleCount || 8);
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
  pagxNormalizeTiming,
  pagxBuildCanonicalAnimation,
  pagxCandidateElements,
  pagxBuildKeyframesIndex,
  pagxCollectWAAPI,
  pagxCollectCSS,
  pagxSampleTimeline,
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
      sampleCount: options.sampleCount || 8,
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
