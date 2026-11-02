// Copyright (C) 2026 Tencent. All rights reserved.
'use strict';
const vm = require('vm');
const { HELPERS_SRC, PAYLOAD_CONSTANTS_SRC } = require('../dist/lib/browser-snapshot');
function helpers() {
  const context = { Node: { TEXT_NODE: 3, ELEMENT_NODE: 1 }, getComputedStyle: element => element.cs,
    document: { documentElement: { getAttribute: () => 'en' } } };
  vm.createContext(context);
  vm.runInContext(PAYLOAD_CONSTANTS_SRC + '\n' + HELPERS_SRC + '\n;globalThis.testHelpers={emitInlineRunMarkup,buildLanguageAttribute};', context);
  return context.testHelpers;
}
function style(axes, weight = '400') {
  return { getPropertyValue: name => ({ 'font-family': '"PingFang SC", sans-serif', 'font-weight': weight,
    'font-style': 'normal', 'font-variation-settings': axes, opacity: '1' }[name] || '') };
}
test('preserves an inline axis-only request even when ordinary text styles are identical', () => {
  const h = helpers();
  const child = { nodeType: 1, cs: style('"wght" 650'), childNodes: [{ nodeType: 3, nodeValue: 'B' }], closest: () => null };
  const parent = { closest: () => null, childNodes: [{ nodeType: 3, nodeValue: 'A' }, child] };
  const markup = h.emitInlineRunMarkup(parent, style('"wght" 400'));
  expect(markup).not.toContain('data-pagx-font-request');
  expect(markup).toContain("font-variation-settings: 'wght' 650");
  expect(markup).toContain('>B</span>');
});
test('retains different inherited language metadata and preserves numeric 650 in the browser payload', () => {
  const h = helpers();
  const child = { nodeType: 1, cs: style('normal'), childNodes: [{ nodeType: 3, nodeValue: 'B' }], closest: () => ({ getAttribute: () => 'ar' }) };
  const parent = { closest: () => null, childNodes: [child] };
  expect(h.emitInlineRunMarkup(parent, style('normal'))).toContain("lang='ar'");
  child.cs = style('normal', '650');
  expect(h.emitInlineRunMarkup(parent, style('normal'))).toContain('font-weight: 650');
});

test('emits explicit normal values when an inline child resets inherited font properties', () => {
  const h = helpers();
  const child = { nodeType: 1, cs: style('normal', '400'),
    childNodes: [{ nodeType: 3, nodeValue: 'B' }], closest: () => null };
  const parent = { closest: () => null, childNodes: [child] };
  const markup = h.emitInlineRunMarkup(parent, style('"wght" 650', '700'));
  expect(markup).toContain('font-weight: 400');
  expect(markup).toContain('font-variation-settings: normal');
});
