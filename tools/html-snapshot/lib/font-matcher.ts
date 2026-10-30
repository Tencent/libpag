// Copyright (C) 2026 Tencent. All rights reserved.

import { FontMappingError, FontMappingErrorCode, fontRequestKey } from './font-metadata';
import type { FontTarget, HtmlFontRequest } from './font-metadata';

export interface AvailableFont {
  fontName: FontTarget;
}

const GENERIC_FAMILIES = new Set([
  'serif', 'sans-serif', 'monospace', 'cursive', 'fantasy', 'system-ui',
  'ui-serif', 'ui-sans-serif', 'ui-monospace', 'ui-rounded', 'emoji', 'math',
  'fangsong', '-apple-system', 'blinkmacsystemfont',
]);

/** Validate the frontend catalog before opening a browser or starting native conversion. */
export function validateAvailableFonts(value: unknown): string[] {
  if (!Array.isArray(value)) return ['availableFonts must be an array'];
  if (value.length > 100000) return ['availableFonts exceeds 100000 faces'];
  for (const [index, entry] of value.entries()) {
    const name = entry?.fontName;
    if (!name || typeof name !== 'object' ||
        !['family', 'style'].every(key => typeof name[key] === 'string' &&
          name[key].trim().length > 0 && name[key].length <= 256 && !/[\x00-\x1f\x7f]/.test(name[key]))) {
      return [`availableFonts[${index}].fontName requires non-empty family/style strings of at most 256 characters`];
    }
  }
  return [];
}

function normalizeName(value: string): string {
  return value.normalize('NFKC').trim().toLowerCase().replace(/\s+/g, ' ');
}

function compactName(value: string): string {
  return normalizeName(value).replace(/[\s_-]+/g, '');
}

function nameScore(left: string, right: string): number {
  const minLength = Math.min(left.length, right.length);
  const maxLength = Math.max(left.length, right.length);
  if (minLength < 6 || maxLength > 64) return 0;
  if ((left.endsWith(right) || right.endsWith(left)) && minLength / maxLength >= 0.5) return 0.94;
  if (Math.abs(left.length - right.length) > 2) return 0;
  let previous = Array.from({ length: right.length + 1 }, (_, index) => index);
  for (let i = 1; i <= left.length; i++) {
    const current = [i];
    for (let j = 1; j <= right.length; j++) {
      current[j] = Math.min(current[j - 1] + 1, previous[j] + 1,
        previous[j - 1] + (left[i - 1] === right[j - 1] ? 0 : 1));
    }
    if (Math.min(...current) > 2) return 0;
    previous = current;
  }
  return previous[right.length] <= 2 ? 1 - previous[right.length] / maxLength : 0;
}

function faceWeight(style: string): number | null {
  const name = compactName(style).replace(/italic|oblique/g, '');
  const weights: Record<string, number> = {
    thin: 100, hairline: 100, extralight: 200, ultralight: 200, light: 300,
    regular: 400, normal: 400, book: 400, roman: 400, '': 400,
    medium: 500, semibold: 600, demibold: 600, bold: 700,
    extrabold: 800, ultrabold: 800, black: 900, heavy: 900,
  };
  if (Object.prototype.hasOwnProperty.call(weights, name)) return weights[name];
  return /^\d{1,4}$/.test(name) && Number(name) >= 1 && Number(name) <= 1000 ? Number(name) : null;
}

function faceRank(style: string, request: HtmlFontRequest): number[] {
  const normalized = normalizeName(style);
  const slant = normalized.includes('italic') ? 'italic' : normalized.includes('oblique') ? 'oblique' : 'normal';
  const slantRank = slant === request.style ? 0 : slant !== 'normal' && request.style !== 'normal' ? 1 : 2;
  const weight = faceWeight(style);
  return [slantRank, weight === null ? 1 : 0, weight === null ? 0 : Math.abs(weight - request.weight),
    weight === null ? 0 : request.weight <= 500 ? weight : -weight];
}

/** Select only catalog faces; ambiguous or weak family matches leave the source unchanged. */
export class FontMatcher {
  private families = new Map<string, FontTarget[]>();
  private normalized = new Map<string, Set<string>>();
  private compact = new Map<string, Set<string>>();
  private familyCache = new Map<string, string | null>();
  private requestCache = new Map<string, FontTarget | null>();

  constructor(fonts: AvailableFont[]) {
    const errors = validateAvailableFonts(fonts);
    if (errors.length) throw new FontMappingError(FontMappingErrorCode.InvalidAvailableFonts, errors.join('; '));
    const seen = new Set<string>();
    for (const { fontName } of fonts) {
      const { family, style } = fontName;
      const key = JSON.stringify([family, style]);
      if (seen.has(key)) continue;
      seen.add(key);
      if (!this.families.has(family)) this.families.set(family, []);
      this.families.get(family)!.push({ family, style });
      for (const [index, name] of [[this.normalized, normalizeName(family)], [this.compact, compactName(family)]] as const) {
        if (!index.has(name)) index.set(name, new Set());
        index.get(name)!.add(family);
      }
    }
  }

  match(request: HtmlFontRequest): FontTarget | null {
    // Preserve exact spelling here: normalized catalog collisions must not poison exact lookups.
    const key = JSON.stringify([request.families, fontRequestKey(request)]);
    if (!this.requestCache.has(key)) this.requestCache.set(key, this.matchRequest(request));
    const target = this.requestCache.get(key);
    return target ? { ...target } : null;
  }

  private matchRequest(request: HtmlFontRequest): FontTarget | null {
    for (const candidate of request.families) {
      if (GENERIC_FAMILIES.has(normalizeName(candidate))) continue;
      if (!this.familyCache.has(candidate)) this.familyCache.set(candidate, this.findFamily(candidate));
      const family = this.familyCache.get(candidate);
      if (!family) continue;
      const faces = this.families.get(family)!;
      let best = faces[0];
      let bestRank = faceRank(best.style, request);
      for (const face of faces.slice(1)) {
        const rank = faceRank(face.style, request);
        let difference = 0;
        for (let index = 0; index < rank.length && !difference; index++) difference = rank[index] - bestRank[index];
        if (difference < 0 || (difference === 0 && face.style < best.style)) {
          best = face;
          bestRank = rank;
        }
      }
      return best;
    }
    return null;
  }

  private findFamily(name: string): string | null {
    if (this.families.has(name)) return name;
    const normalized = this.normalized.get(normalizeName(name));
    if (normalized) return normalized.size === 1 ? [...normalized][0] : null;
    const compact = compactName(name);
    const exact = this.compact.get(compact);
    if (exact) return exact.size === 1 ? [...exact][0] : null;
    let best: string | null = null, bestScore = 0, runnerUp = 0;
    for (const [candidate, families] of this.compact) {
      const score = nameScore(compact, candidate);
      for (const family of families) {
        if (score > bestScore) {
          runnerUp = bestScore;
          bestScore = score;
          best = family;
        } else if (score > runnerUp) runnerUp = score;
      }
    }
    return bestScore >= 0.85 && bestScore - runnerUp >= 0.08 ? best : null;
  }
}
