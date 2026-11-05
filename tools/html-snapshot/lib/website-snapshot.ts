// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
import * as path from 'node:path';
import * as fs from 'node:fs';
import { fileURLToPath } from 'node:url';
import { DOMParser, XMLSerializer } from '@xmldom/xmldom';
import { lookup } from 'mime-types';

/** Residual remote/missing references must not reach the native filesystem resolver. */
export function portableWebsiteSnapshot(html: string, imageDirectory: string | undefined, warnings: string[],
    capturedFiles: Map<string, string> = new Map(), depth = 0): string {
  const doc = new DOMParser().parseFromString(html, 'text/html');
  const inline = (file: string) => `data:${lookup(file) || 'application/octet-stream'};base64,${fs.readFileSync(file).toString('base64')}`;
  const resource = (value: string): string | undefined => {
    if (/^data:image\/svg\+xml[;,]/i.test(value)) {
      try {
        if (depth >= 8) throw new Error('Nested SVG resource limit');
        const comma = value.indexOf(','), payload = value.slice(comma + 1);
        const svg = /;base64/i.test(value.slice(0, comma)) ? Buffer.from(payload, 'base64').toString('utf8') : decodeURIComponent(payload);
        return 'data:image/svg+xml;base64,' + Buffer.from(portableWebsiteSnapshot(svg, imageDirectory, warnings, capturedFiles, depth + 1)).toString('base64');
      } catch (_) { warnings.push('Invalid nested SVG resource omitted'); return undefined; }
    }
    if (!value || value.startsWith('#') || /^data:/i.test(value)) return value;
    const captured = capturedFiles.get(value);
    if (captured) return inline(captured);
    if (imageDirectory && !/^https?:/i.test(value)) {
      try {
        const file = value.startsWith('file:') ? fileURLToPath(value) : path.resolve(value);
        const relative = path.relative(path.resolve(imageDirectory), file);
        if (relative && !relative.startsWith('..' + path.sep) && relative !== '..' && !path.isAbsolute(relative)) {
          return inline(file);
        }
      } catch (_) { /* report below */ }
    }
    warnings.push(`Uncaptured resource omitted: ${value}`);
    return undefined;
  };
  const css = (value: string) => value.replace(/url\(\s*(?:"([^"]*)"|'([^']*)'|([^)]*))\s*\)/gi,
    (_original, quoted, single, bare) => {
      const resolved = resource((quoted ?? single ?? bare).trim());
      return resolved === undefined ? 'none' : `url("${resolved}")`;
    });
  for (const element of Array.from(doc.getElementsByTagName('*'))) {
    if (element.localName === 'style') element.textContent = css(element.textContent || '');
    for (const name of ['src', 'href', 'xlink:href']) {
      const value = element.getAttribute(name);
      if (value) {
        const resolved = resource(value);
        if (resolved === undefined) element.removeAttribute(name);
        else if (resolved !== value) element.setAttribute(name, resolved);
      }
    }
    for (const attribute of Array.from(element.attributes)) {
      if (!/url\(/i.test(attribute.value)) continue;
      element.setAttribute(attribute.name, css(attribute.value));
    }
  }
  return new XMLSerializer().serializeToString(doc);
}
