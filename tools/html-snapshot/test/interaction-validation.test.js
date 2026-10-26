// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const { parseScenario, selectCheckpoint, runScenario } = require('../dist/lib/interaction');
const capture = { type: 'capture', name: 'state' };
const scenario = steps => ({ version: 1, steps });
const target = { css: '#input' };

test.each([
  [null, /scenario must be an object/],
  [[], /scenario must be an object/],
  [{ version: 1, viewport: [] , steps: [capture] }, /viewport must be an object/],
  [{ version: 1, viewport: { scale: 2 }, steps: [capture] }, /unknown field/],
  [{ version: 1, steps: [] }, /1–256/],
  [{ version: 1, steps: 'capture' }, /1–256/],
  [scenario(Array(257).fill(capture)), /1–256/],
  [scenario([null]), /step 1 must be an object/],
  [scenario([{ type: '' }]), /non-empty string/],
  [scenario([{ type: 'constructor' }]), /unsupported action/],
  [scenario([{ type: 'click', target, value: 'unexpected' }, capture]), /unknown field/],
  [scenario([{ type: 'click', target: {} }, capture]), /exactly one/],
  [scenario([{ type: 'click', target: { css: '' } }, capture]), /non-empty string/],
  [scenario([{ type: 'click', target: { css: '#a', name: 'button' } }, capture]), /name requires role/],
  [scenario([{ type: 'click', target: { css: '#a', frame: 3 } }, capture]), /non-empty string/],
  [scenario([{ type: 'check', target, value: 'true' }, capture]), /must be boolean/],
  [scenario([{ type: 'press', target, key: '' }, capture]), /key/],
  [scenario([{ type: 'click', target, url: '' }, capture]), /url/],
  [scenario([{ type: 'scroll' }, capture]), /requires x or y/],
  [scenario([{ type: 'scroll', x: Infinity }, capture]), /between/],
  [scenario([{ type: 'scroll', y: -10000001 }, capture]), /between/],
  [scenario([{ type: 'waitFor', target }, capture]), /exactly one condition/],
  [scenario([{ type: 'waitFor', target, state: 'enabled' }, capture]), /invalid visibility state/],
  [scenario([{ type: 'waitFor', target, text: 0 }, capture]), /text must be a string/],
  [scenario([{ type: 'waitFor', target, attribute: null }, capture]), /attribute must be an object/],
  [scenario([{ type: 'waitFor', target, attribute: { name: '', value: '' } }, capture]), /non-empty string/],
  [scenario([{ type: 'waitFor', target, attribute: { name: 'ready', value: true } }, capture]), /value must be a string/],
  [scenario([{ ...capture, extent: 'all' }]), /invalid extent/],
  [scenario([{ ...capture, unsupported: 'ignore' }]), /invalid unsupported policy/],
  [scenario([{ ...capture, name: 'a'.repeat(81) }]), /invalid checkpoint name/],
  [scenario([{ type: 'click', target }]), /1–32 named capture/],
  [scenario(Array.from({ length: 33 }, (_, i) => ({ ...capture, name: `state${i}` }))), /1–32 named capture/],
  [scenario([{ ...capture, timeoutMs: 0 }]), /step 1.timeoutMs/],
])('invalid scenarios fail with actionable diagnostics (%j)', (input, message) => {
  expect(() => parseScenario(input)).toThrow(message);
});

test.each([
  ['timeoutMs', 600001], ['stepTimeoutMs', NaN], ['stableMs', -1], ['stableMs', 5001],
])('validates %s timing bounds', (key, value) => {
  expect(() => parseScenario({ ...scenario([capture]), [key]: value })).toThrow(key);
});

test.each([0, 16385, 1.5, '100', Infinity])('validates both viewport dimensions (%s)', value => {
  for (const dimension of ['width', 'height']) {
    expect(() => parseScenario({ ...scenario([capture]), viewport: { [dimension]: value } })).toThrow(dimension);
  }
});

test('accepts boundary values and empty form/text conditions without changing caller data', () => {
  const steps = [
    { type: 'fill', target, value: '' }, { type: 'type', target, value: '' },
    { type: 'select', target, value: '' }, { type: 'check', target, value: false },
    { type: 'press', target, key: 'Enter', url: '#next' },
    { type: 'scroll', x: -10000000, y: 10000000 },
    { type: 'waitFor', url: '#next' }, { type: 'waitFor', target, text: '' },
    { type: 'waitFor', target, attribute: { name: 'data-ready', value: '' } },
    { ...capture, extent: 'document', unsupported: 'rasterize', timeoutMs: 300000 },
  ];
  const input = { ...scenario(steps), viewport: { width: 1, height: 16384 },
    timeoutMs: 600000, stepTimeoutMs: 300000, stableMs: 0 };
  const before = JSON.stringify(input);
  expect(parseScenario(input)).toEqual(input);
  expect(JSON.stringify(input)).toBe(before);
});

test('checkpoint selection never silently drops requested states', () => {
  expect(selectCheckpoint(parseScenario(scenario([capture])))).toBe('state');
  const multiple = parseScenario(scenario([capture, { ...capture, name: 'later' }]));
  expect(() => selectCheckpoint(multiple)).toThrow('Multiple checkpoints');
  expect(() => selectCheckpoint(multiple, 'missing')).toThrow('Unknown checkpoint');
  expect(selectCheckpoint(multiple, 'later')).toBe('later');
});

test.each([
  [{ downloadFonts: true }, /fontDir/], [{ downloadImages: true }, /imageDir/],
  [{ checkpoint: 'missing' }, /Unknown checkpoint/],
])('rejects acquisition options before allocating a browser session (%j)', async (options, message) => {
  const browser = { createBrowserContext: jest.fn() };
  await expect(runScenario({ engine: 'puppeteer', browser }, 'https://example.test', scenario([capture]), options))
    .rejects.toThrow(message);
  expect(browser.createBrowserContext).not.toHaveBeenCalled();
});
