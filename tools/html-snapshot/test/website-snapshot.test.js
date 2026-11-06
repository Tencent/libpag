// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');
const { DOMParser } = require('@xmldom/xmldom');
const { portableWebsiteSnapshot } = require('../dist/lib/website-snapshot');
let dir;
beforeEach(() => { dir = fs.mkdtempSync(path.join(os.tmpdir(), 'website-snapshot-')); });
afterEach(() => fs.rmSync(dir, { recursive: true, force: true }));

test('inlines captured files and file URLs, preserving fragments and existing data URLs', () => {
  const image = path.join(dir, 'image.png'), unknown = path.join(dir, 'asset.unknown');
  fs.writeFileSync(image, 'image'); fs.writeFileSync(unknown, 'unknown');
  const warnings = [];
  const output = portableWebsiteSnapshot(`<html><body><img src="${pathToFileURL(image)}"/>` +
    '<img src="data:image/png;base64,AAA="/><a href="#section">link</a><img src="https://site/asset"/>' +
    '<div style="background:url(\'https://site/image\');mask:url(#clip)"></div></body></html>', dir, warnings,
  new Map([['https://site/asset', unknown], ['https://site/image', image]]));
  expect(output).toContain('data:image/png;base64,aW1hZ2U=');
  expect(output).toContain('data:application/octet-stream;base64,dW5rbm93bg==');
  expect(output).toContain('data:image/png;base64,AAA=');
  expect(output).toContain('href="#section"');
  expect(output).toContain('#clip');
  expect(output).not.toContain('https://site'); expect(warnings).toEqual([]);
});

test('omits missing local files, parent traversal, image directories and uncaptured stylesheet references', () => {
  const warnings = [];
  const outside = path.join(path.dirname(dir), 'outside.png');
  const output = portableWebsiteSnapshot(`<html><head><style>.x{background:url('${outside}')}</style></head>` +
    `<body><img src="${path.join(dir, 'missing.png')}"/><img src="${dir}"/>` +
    '<svg xmlns:xlink="http://www.w3.org/1999/xlink"><image xlink:href="https://blocked/image.png"/></svg>' +
    '<div style="background:url()"></div></body></html>', dir, warnings);
  expect(output).not.toContain('outside.png'); expect(output).not.toContain('missing.png');
  expect(output).not.toContain('https://blocked');
  expect(warnings).toHaveLength(4);
});

test('sanitizes percent-encoded SVG payloads, bounds nesting and drops invalid escapes', () => {
  const svg = '<svg xmlns="http://www.w3.org/2000/svg"><image href="https://blocked/image.png"/></svg>';
  const warnings = [];
  const result = portableWebsiteSnapshot(`<html><body><img src="data:image/svg+xml,${encodeURIComponent(svg)}"/>` +
    '<img src="data:image/svg+xml,%ZZ"/></body></html>', undefined, warnings);
  const doc = new DOMParser().parseFromString(result, 'text/html');
  const images = doc.getElementsByTagName('img');
  const payload = images[0].getAttribute('src').split(',')[1];
  expect(Buffer.from(payload, 'base64').toString()).not.toContain('https://blocked');
  expect(images[1].hasAttribute('src')).toBe(false);
  expect(warnings).toContain('Invalid nested SVG resource omitted');
  let nested = '<svg xmlns="http://www.w3.org/2000/svg"/>';
  for (let i = 0; i < 10; i++) nested = `<svg xmlns="http://www.w3.org/2000/svg"><image href="data:image/svg+xml;base64,${Buffer.from(nested).toString('base64')}"/></svg>`;
  const limited = [];
  expect(() => portableWebsiteSnapshot(nested, undefined, limited)).not.toThrow();
  expect(limited).toEqual(['Invalid nested SVG resource omitted']);
});
