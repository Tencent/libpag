// Copyright (C) 2026 Tencent. All rights reserved.
// Internal original HTML font requests used during conversion.

export type CssFontStyle = 'normal' | 'italic' | 'oblique';

export interface HtmlFontRequest {
  families: string[];
  weight: number;
  style: CssFontStyle;
  variations?: Record<string, number>;
  lang?: string;
}

export interface HtmlFontMetadataEntry extends HtmlFontRequest {
  /** Display label only; matching still uses the exact numeric weight. */
  weightName: string;
}

/** Source requests captured from snapshot text before native conversion or font replacements. */
export interface HtmlFontMetadata {
  version: 1;
  fonts: HtmlFontMetadataEntry[];
  /** Snapshot text runs without a captured HTML request (for example, inline SVG text). */
  missingRequests: number;
}

export interface FontTarget {
  family: string;
  style: string;
}

export const FontMappingErrorCode = {
  InvalidAvailableFonts: 'INVALID_AVAILABLE_FONTS',
  FontMetadataMismatch: 'FONT_METADATA_MISMATCH',
} as const;

export class FontMappingError extends Error {
  constructor(public code: typeof FontMappingErrorCode[keyof typeof FontMappingErrorCode], message: string) {
    super(`${code}: ${message}`);
    this.name = 'FontMappingError';
  }
}

/** Self-contained so the browser snapshot can serialize this function into its payload. */
export function parseFontFamilyStack(value: string): string[] {
  const families: string[] = [];
  let current = '';
  let quote = '';
  for (let i = 0; i < String(value || '').length; i++) {
    const ch = value[i];
    if (ch === '\\') {
      const tail = value.slice(i + 1);
      const hex = /^[0-9a-fA-F]{1,6}/.exec(tail);
      if (hex) {
        const point = parseInt(hex[0], 16);
        current += String.fromCodePoint(point === 0 || point > 0x10ffff ||
          (point >= 0xd800 && point <= 0xdfff) ? 0xfffd : point);
        i += hex[0].length;
        if (/\s/.test(value[i + 1] || '')) {
          i++;
          if (value[i] === '\r' && value[i + 1] === '\n') i++;
        }
      } else if (i + 1 < value.length) {
        const next = value[++i];
        if (next !== '\n' && next !== '\r' && next !== '\f') current += next;
        if (next === '\r' && value[i + 1] === '\n') i++;
      } else {
        throw new Error('font-family ends with an escape');
      }
    } else if (quote) {
      if (ch === quote) quote = '';
      else current += ch;
    } else if (ch === '"' || ch === "'") {
      quote = ch;
    } else if (ch === ',') {
      if (!current.trim()) throw new Error('font-family contains an empty family');
      families.push(current.trim());
      current = '';
    } else {
      current += ch;
    }
  }
  if (quote) throw new Error('font-family contains an unclosed quote');
  if (current.trim()) families.push(current.trim());
  else if (families.length) throw new Error('font-family ends with a comma');
  return families;
}

export function fontWeightNumber(value: unknown): number | null {
  if (typeof value !== 'number' && typeof value !== 'string') return null;
  const text = String(value).trim().toLowerCase();
  const weight = text === 'normal' ? 400 : text === 'bold' ? 700 : text ? Number(text) : NaN;
  return Number.isFinite(weight) && weight >= 1 && weight <= 1000 ? weight : null;
}

export function fontRequestKey(request: HtmlFontRequest): string {
  return JSON.stringify([
    request.families.map((family) => family.trim().normalize('NFC').toLowerCase()),
    request.weight,
    request.style,
  ]);
}

function isObject(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

function isText(value: unknown): value is string {
  return typeof value === 'string' && value.trim().length > 0;
}

export function validateFontRequest(value: unknown): value is HtmlFontRequest {
  return isObject(value) && Array.isArray(value.families) && value.families.length > 0 &&
    value.families.every(isText) && typeof value.weight === 'number' &&
    fontWeightNumber(value.weight) !== null && ['normal', 'italic', 'oblique'].includes(String(value.style)) &&
    (value.lang === undefined || isText(value.lang)) &&
    (value.variations === undefined || (isObject(value.variations) &&
      Object.entries(value.variations).every(([axis, number]) =>
        /^[A-Za-z0-9]{4}$/.test(axis) && typeof number === 'number' && Number.isFinite(number))));
}

const FONT_WEIGHT_NAMES: Readonly<Record<number, string>> = {
  100: 'Thin',
  200: 'ExtraLight',
  300: 'Light',
  400: 'Regular',
  500: 'Medium',
  600: 'SemiBold',
  700: 'Bold',
  800: 'ExtraBold',
  900: 'Black',
};

/** Deduplicate without losing explicit axes or language, in first-use order. */
export function collectFontMetadata(
  requests: readonly HtmlFontRequest[], missingRequests = 0,
): HtmlFontMetadata {
  const fonts = new Map<string, HtmlFontMetadataEntry>();
  for (const request of requests) {
    if (!validateFontRequest(request)) {
      throw new FontMappingError(FontMappingErrorCode.FontMetadataMismatch, 'invalid source font request');
    }
    const axes = Object.entries(request.variations || {}).sort(([a], [b]) => a.localeCompare(b));
    const key = JSON.stringify([fontRequestKey(request), axes, request.lang || '']);
    if (fonts.has(key)) continue;
    fonts.set(key, {
      families: [...request.families],
      weight: request.weight,
      weightName: FONT_WEIGHT_NAMES[request.weight] || String(request.weight),
      style: request.style,
      ...(axes.length ? { variations: Object.fromEntries(axes) } : {}),
      ...(request.lang ? { lang: request.lang } : {}),
    });
  }
  return { version: 1, fonts: [...fonts.values()], missingRequests };
}
