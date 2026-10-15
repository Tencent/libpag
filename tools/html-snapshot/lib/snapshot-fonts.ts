// Copyright (C) 2026 Tencent. All rights reserved.

import { DOMParser } from '@xmldom/xmldom';
import type { Node, Element } from '@xmldom/xmldom';
import {
  collectFontMetadata, fontRequestKey, parseFontFamilyStack, fontWeightNumber,
  validateFontMap, validateFontRequest, FontMappingError, FontMappingErrorCode,
} from './font-metadata';
import type { FontMapRule, HtmlFontMetadata, HtmlFontRequest } from './font-metadata';

/** Read the captured requests before any native import, resolving inherited span requests. */
export function collectSnapshotFontMetadata(html: string): HtmlFontMetadata {
  const doc = new DOMParser().parseFromString(html, 'text/html');
  const requests: HtmlFontRequest[] = [];
  let missingRequests = 0;

  function visit(node: Node, inherited?: HtmlFontRequest): void {
    let request = inherited;
    if (node.nodeType === 1) {
      const element = node as Element;
      if (['head', 'style', 'script'].includes(element.tagName.toLowerCase())) return;
      const raw = element.getAttribute('data-pagx-font-request');
      if (raw !== null) {
        let parsed: unknown;
        try {
          parsed = JSON.parse(raw);
        } catch (_error) {
          throw new FontMappingError(FontMappingErrorCode.FontMetadataMismatch, 'invalid snapshot font request');
        }
        if (!validateFontRequest(parsed)) {
          throw new FontMappingError(FontMappingErrorCode.FontMetadataMismatch, 'invalid snapshot font request');
        }
        request = parsed;
      }
    } else if (node.nodeType === 3 && node.nodeValue?.trim()) {
      if (request) requests.push(request);
      else missingRequests++;
    }
    for (let child = node.firstChild; child; child = child.nextSibling) visit(child, request);
  }

  visit(doc);
  return collectFontMetadata(requests, missingRequests);
}

/** Reject rules for fonts outside the prepared snapshot before starting native conversion. */
export function validateSnapshotFontMap(fontMap: unknown, metadata: HtmlFontMetadata): string[] {
  const errors = validateFontMap(fontMap);
  if (errors.length) return errors;
  const sources = new Set(metadata.fonts.map(fontRequestKey));
  for (const [index, rule] of (fontMap as FontMapRule[]).entries()) {
    const key = fontRequestKey({
      families: parseFontFamilyStack(rule.from['font-family']),
      weight: fontWeightNumber(rule.from['font-weight'])!,
      style: rule.from['font-style'],
    });
    if (!sources.has(key)) errors.push(`font map[${index}].from is not in the prepared snapshot`);
  }
  return errors;
}
