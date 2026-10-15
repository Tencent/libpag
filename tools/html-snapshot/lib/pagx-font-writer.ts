// Copyright (C) 2026 Tencent. All rights reserved.

import * as fs from 'fs';
import { randomUUID } from 'crypto';
import { DOMParser, XMLSerializer } from '@xmldom/xmldom';
import type { Document, Node } from '@xmldom/xmldom';
import { FontMatcher } from './font-matcher';
import {
  FontMapRule, FontMappingError, FontMappingErrorCode, validateFontRequest,
} from './font-metadata';

const REQUEST_ATTR = 'data-html-font-request';

export interface FontRewriteResult {
  replaced: number;
  unmatched: number;
  missingRequests: number;
}

function parseDocument(xml: string): Document {
  try {
    const doc = new DOMParser({ onError: (_level, message) => { throw new Error(message); } }).parseFromString(xml, 'application/xml');
    if (!doc.documentElement || doc.documentElement.tagName !== 'pagx') throw new Error('missing pagx root');
    return doc;
  } catch (error) {
    throw new FontMappingError(FontMappingErrorCode.PagxFontRewriteFailed, (error as Error).message);
  }
}

/** Compare all content except the two rewritten font attributes and temporary requests. */
function invariant(node: Node): unknown {
  const attributes: Array<[string, string]> = [];
  if (node.nodeType === 1) {
    const element = node as import('@xmldom/xmldom').Element;
    for (let i = 0; i < element.attributes.length; i++) {
      const attribute = element.attributes.item(i)!;
      if (element.tagName === 'Text' && [REQUEST_ATTR, 'fontFamily', 'fontStyle'].includes(attribute.name)) continue;
      attributes.push([attribute.name, attribute.value]);
    }
    attributes.sort((a, b) => a[0].localeCompare(b[0]));
  }
  const children: unknown[] = [];
  for (let child = node.firstChild; child; child = child.nextSibling) children.push(invariant(child));
  return [node.nodeType, node.nodeName, node.nodeValue, attributes, children];
}

/** Rewrite each Text directly from its own source request after embedding, without Text IDs. */
export function rewritePagxFonts(input: {
  inputPath: string;
  outputPath: string;
  fontMap: FontMapRule[];
}): FontRewriteResult {
  const matcher = new FontMatcher(input.fontMap);
  const doc = parseDocument(fs.readFileSync(input.inputPath, 'utf8'));
  const originalInvariant = JSON.stringify(invariant(doc));
  const result = { replaced: 0, unmatched: 0, missingRequests: 0 };
  const texts = doc.getElementsByTagName('Text');
  for (let i = 0; i < texts.length; i++) {
    const text = texts.item(i)!;
    const raw = text.getAttribute(REQUEST_ATTR);
    if (raw === null) {
      if ((text.getAttribute('text') || '').trim()) result.missingRequests++;
      continue;
    }
    let request: unknown;
    try {
      request = JSON.parse(raw);
    } catch (error) {
      throw new FontMappingError(FontMappingErrorCode.FontMetadataMismatch, `invalid font request: ${(error as Error).message}`);
    }
    if (!validateFontRequest(request)) {
      throw new FontMappingError(FontMappingErrorCode.FontMetadataMismatch, 'invalid source font request');
    }
    const target = matcher.match(request);
    if (target) {
      text.setAttribute('fontFamily', target.family);
      text.setAttribute('fontStyle', target.style);
      result.replaced++;
    } else {
      result.unmatched++;
    }
    text.removeAttribute(REQUEST_ATTR);
  }
  const xml = new XMLSerializer().serializeToString(doc);
  const verified = parseDocument(xml);
  if (JSON.stringify(invariant(verified)) !== originalInvariant) {
    throw new FontMappingError(FontMappingErrorCode.PagxFontRewriteFailed, 'rewrite changed non-font PAGX content');
  }
  const tempPath = `${input.outputPath}.tmp-${randomUUID()}`;
  try {
    fs.writeFileSync(tempPath, xml, 'utf8');
    fs.renameSync(tempPath, input.outputPath);
  } catch (error) {
    if (fs.existsSync(tempPath)) fs.unlinkSync(tempPath);
    throw new FontMappingError(FontMappingErrorCode.PagxFontRewriteFailed, (error as Error).message);
  }
  return result;
}
