// Copyright (C) 2026 Tencent. All rights reserved.

import { DOMParser } from '@xmldom/xmldom';
import type { Document as XmlDocument, Element, Node } from '@xmldom/xmldom';
import { FontMatcher, type AvailableFont } from './font-matcher';
import {
  fontFaceRenderedStyle, fontFaceRenderedWeight, normalizeFontFaceStyle,
  normalizeFontFaceWeight, selectFontFace,
} from './font-face-pass';
import type { FontFaceDescriptor } from './font-face-pass';
import {
  collectFontMetadata, parseFontFamilyStack, fontWeightNumber,
} from './font-metadata';
import type { HtmlFontMetadata, HtmlFontRequest } from './font-metadata';

export interface FontMappingResult {
  replaced: number;
  unmatched: number;
  missingRequests: number;
}

const FONT_PROPERTIES = ['font-family', 'font-weight', 'font-style', 'font-variation-settings'];

// Snapshot styles are computed longhands. Respect quoted family names and URLs when splitting.
function parseStyle(value: string): Map<string, string> {
  const result = new Map<string, string>();
  let start = 0, colon = -1, depth = 0, quote = '';
  for (let i = 0; i <= value.length; i++) {
    const ch = value[i] || ';';
    if (ch === '\\') { i++; continue; }
    if (quote) { if (ch === quote) quote = ''; continue; }
    if (ch === '"' || ch === "'") { quote = ch; continue; }
    if (ch === '(') depth++;
    if (ch === ')') depth--;
    if (depth !== 0) continue;
    if (ch === ':' && colon < 0) colon = i;
    if (ch !== ';') continue;
    if (colon >= start) {
      result.set(value.slice(start, colon).trim().toLowerCase(),
        value.slice(colon + 1, i).trim());
    }
    start = i + 1;
    colon = -1;
  }
  return result;
}

function cssValue(value: string): string {
  return value.replace(/\s*!important\s*$/i, '').trim();
}

function readRequest(styles: Map<string, string>, lang: string): HtmlFontRequest | undefined {
  const family = styles.get('font-family');
  if (!family) return undefined;
  const families = parseFontFamilyStack(family);
  if (!families.length) return undefined;
  const rawStyle = styles.get('font-style') || 'normal';
  const request: HtmlFontRequest = {
    families,
    weight: fontWeightNumber(styles.get('font-weight') || '400') ?? 400,
    style: rawStyle.startsWith('oblique') ? 'oblique' : rawStyle === 'italic' ? 'italic' : 'normal',
  };
  if (lang) request.lang = lang;
  const axes: Record<string, number> = {};
  const pattern = /["']([A-Za-z0-9]{4})["']\s+(-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)/g;
  for (const match of (styles.get('font-variation-settings') || '').matchAll(pattern)) {
    axes[match[1]] = Number(match[2]);
  }
  if (Object.keys(axes).length) request.variations = axes;
  return request;
}

function skipCssComment(text: string, index: number): number {
  const end = text.indexOf('*/', index + 2);
  return end < 0 ? text.length : end + 2;
}

// Quotes and comments hide braces and semicolons from every scan below: a `data:` URI keeps
// its semicolons, a family name may contain `/*`, and a selector may quote `@font-face`.
function skipCssString(text: string, index: number): number {
  const quote = text[index];
  for (let i = index + 1; i < text.length; i++) {
    if (text[i] === '\\') { i++; continue; }
    if (text[i] === quote) return i + 1;
  }
  return text.length;
}

function stripCssComments(text: string): string {
  let out = '';
  for (let i = 0; i < text.length; i++) {
    const ch = text[i];
    if (ch === '"' || ch === "'") {
      const end = skipCssString(text, i);
      out += text.slice(i, end);
      i = end - 1;
      continue;
    }
    if (ch === '/' && text[i + 1] === '*') {
      i = skipCssComment(text, i) - 1;
      continue;
    }
    out += ch;
  }
  return out;
}

// Index of the `}` closing the block opened at `open`, ignoring braces inside strings,
// comments and parentheses (a `url()` source may carry either character).
function findCssBlockEnd(text: string, open: number): number {
  let depth = 0, parens = 0;
  for (let i = open; i < text.length; i++) {
    const ch = text[i];
    if (ch === '/' && text[i + 1] === '*') { i = skipCssComment(text, i) - 1; continue; }
    if (ch === '"' || ch === "'") { i = skipCssString(text, i) - 1; continue; }
    if (ch === '(') parens++;
    else if (ch === ')') parens = Math.max(0, parens - 1);
    else if (parens === 0 && ch === '{') depth++;
    else if (parens === 0 && ch === '}' && --depth === 0) return i;
  }
  return -1;
}

// One `@font-face` rule, reduced to the descriptors that decide which cut gets painted.
function readFontFaceDescriptor(body: string): FontFaceDescriptor | null {
  const styles = parseStyle(stripCssComments(body));
  const family = styles.get('font-family');
  if (!family || !styles.get('src')) return null;
  let name: string;
  try {
    name = parseFontFamilyStack(family)[0] || '';
  } catch (_) {
    return null;
  }
  if (!name) return null;
  return {
    family: name,
    weight: normalizeFontFaceWeight(styles.get('font-weight') || ''),
    style: normalizeFontFaceStyle(styles.get('font-style') || ''),
  };
}

// Walk the `<style>` text as CSS instead of searching it: `@font-face` only counts as an
// at-rule token outside strings, comments and parentheses, and its block is delimited by the
// same rules. A quoted `content:'@font-face'` is therefore not a declaration.
function collectFontFacesFromCss(cssText: string): FontFaceDescriptor[] {
  const faces: FontFaceDescriptor[] = [];
  const text = String(cssText || '');
  let parens = 0;
  for (let i = 0; i < text.length; i++) {
    const ch = text[i];
    if (ch === '/' && text[i + 1] === '*') { i = skipCssComment(text, i) - 1; continue; }
    if (ch === '"' || ch === "'") { i = skipCssString(text, i) - 1; continue; }
    if (ch === '(') { parens++; continue; }
    if (ch === ')') { parens = Math.max(0, parens - 1); continue; }
    if (ch !== '@' || parens !== 0) continue;
    const atRule = /^@([-\w]+)/.exec(text.slice(i, i + 64));
    if (!atRule || atRule[1].toLowerCase() !== 'font-face') continue;
    let cursor = i + atRule[0].length;
    while (cursor < text.length) {
      if (/\s/.test(text[cursor])) { cursor++; continue; }
      if (text[cursor] === '/' && text[cursor + 1] === '*') {
        cursor = skipCssComment(text, cursor);
        continue;
      }
      break;
    }
    if (text[cursor] !== '{') continue;
    const end = findCssBlockEnd(text, cursor);
    if (end < 0) break;
    const face = readFontFaceDescriptor(text.slice(cursor + 1, end));
    if (face) faces.push(face);
    i = end;
  }
  return faces;
}

// Family names are compared after the same normalisation the catalog matcher uses for exact
// lookups, so a rule declared as `"Brand"` answers a run that spells it `brand`.
function familyKey(name: string): string {
  return String(name || '').normalize('NFKC').trim().toLowerCase().replace(/\s+/g, ' ');
}

function collectSnapshotFontFaces(doc: XmlDocument): Map<string, FontFaceDescriptor[]> {
  const faces = new Map<string, FontFaceDescriptor[]>();
  const styles = doc.getElementsByTagName('style');
  for (let i = 0; i < styles.length; i++) {
    const text = styles.item(i)?.textContent || '';
    for (const face of collectFontFacesFromCss(text)) {
      const key = familyKey(face.family);
      const bucket = faces.get(key);
      if (bucket) bucket.push(face);
      else faces.set(key, [face]);
    }
  }
  return faces;
}

// Only the head of the stack can be known to have painted with a declared face: any earlier
// entry may resolve to a font installed on the machine, which the snapshot cannot see, and a
// later family's cut would then rewrite the request for a family that never used it.
function resolveSourceRequest(
  request: HtmlFontRequest, faces: Map<string, FontFaceDescriptor[]>,
): HtmlFontRequest {
  const candidates = faces.get(familyKey(request.families[0]));
  if (!candidates || !candidates.length) return request;
  const face = selectFontFace(candidates, String(request.weight), request.style);
  if (!face) return request;
  const weight = fontFaceRenderedWeight(face, request.weight);
  const style = fontFaceRenderedStyle(face, request.style);
  if (weight === request.weight && style === request.style) return request;
  return { ...request, weight, style: style as HtmlFontRequest['style'] };
}

function readSnapshot(html: string) {
  const doc = new DOMParser().parseFromString(html, 'text/html');
  const faces = collectSnapshotFontFaces(doc);
  const runs: {
    element: Element;
    styles: Map<string, string>;
    request?: HtmlFontRequest;
    matchRequest?: HtmlFontRequest;
  }[] = [];
  function visit(node: Node, inherited: Map<string, string>, language: string, inSvg = false): void {
    if (node.nodeType !== 1) return;
    const element = node as Element;
    if (['head', 'style', 'script'].includes(element.tagName.toLowerCase())) return;
    inSvg ||= element.tagName.toLowerCase() === 'svg';
    const own = parseStyle(element.getAttribute('style') || '');
    const resolved = new Map(inherited);
    for (const property of FONT_PROPERTIES) {
      const value = cssValue(own.get(property) || element.getAttribute(property) || '');
      if (!value || value === 'inherit' || value === 'unset') continue;
      if (value === 'initial') resolved.delete(property);
      else resolved.set(property, value);
    }
    const lang = element.getAttribute('lang') ?? element.getAttribute('xml:lang') ?? language;
    let hasText = false;
    for (let child = element.firstChild; child; child = child.nextSibling) {
      if (child.nodeType === 3 && child.nodeValue?.trim()) hasText = true;
    }
    // Inline SVG retains authored markup rather than computed text styles in the snapshot.
    if (hasText) {
      const request = inSvg ? undefined : readRequest(resolved, lang);
      runs.push({
        element, styles: resolved, request,
        matchRequest: request ? resolveSourceRequest(request, faces) : undefined,
      });
    }
    for (let child = element.firstChild; child; child = child.nextSibling) {
      visit(child, resolved, lang, inSvg);
    }
  }
  if (doc.documentElement) visit(doc.documentElement, new Map(), '');
  return { doc, runs };
}

/** Read original computed CSS from the snapshot before native conversion or font mapping. */
export function collectSnapshotFontMetadata(html: string): HtmlFontMetadata {
  const { runs } = readSnapshot(html);
  return collectFontMetadata(runs.flatMap(run => run.request ? [run.request] : []),
    runs.filter(run => !run.request).length);
}

export interface FontReplacement {
  families: string;
  weight: number;
  style: string;
  targetFamily: string;
  targetStyle: string;
}

/** Match the available catalog before native conversion, keeping the snapshot unchanged. */
export function prepareSnapshotFontReplacements(html: string, availableFonts: AvailableFont[]): {
  replacements: FontReplacement[]; mapping: FontMappingResult;
} {
  const matcher = new FontMatcher(availableFonts);
  const { runs } = readSnapshot(html);
  const mapping = { replaced: 0, unmatched: 0, missingRequests: 0 };
  const replacements = new Map<string, FontReplacement>();
  for (const { styles, request, matchRequest } of runs) {
    if (!request) { mapping.missingRequests++; continue; }
    const target = matcher.match(matchRequest || request);
    if (!target) { mapping.unmatched++; continue; }
    const replacement = {
      families: styles.get('font-family')!, weight: request.weight, style: request.style,
      targetFamily: target.family, targetStyle: target.style,
    };
    replacements.set(JSON.stringify(replacement), replacement);
    mapping.replaced++;
  }
  return { replacements: [...replacements.values()], mapping };
}
