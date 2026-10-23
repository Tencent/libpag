// Copyright (C) 2026 Tencent. All rights reserved.

import { DOMParser } from '@xmldom/xmldom';
import type { Element, Node } from '@xmldom/xmldom';
import { FontMatcher, type AvailableFont } from './font-matcher';
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

function readSnapshot(html: string) {
  const doc = new DOMParser().parseFromString(html, 'text/html');
  const runs: { element: Element; styles: Map<string, string>; request?: HtmlFontRequest }[] = [];
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
    if (hasText) runs.push({ element, styles: resolved,
      request: inSvg ? undefined : readRequest(resolved, lang) });
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
  for (const { styles, request } of runs) {
    if (!request) { mapping.missingRequests++; continue; }
    const target = matcher.match(request);
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
