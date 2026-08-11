'use strict';

const { shouldPinFlexGrowItems } = require('../dist/lib/browser-snapshot');

function style(values = {}) {
  return {
    getPropertyValue(prop) {
      return values[prop] == null ? '' : String(values[prop]);
    },
  };
}

function item(width, grow, extra = {}) {
  return {
    kind: 'element',
    rect: { width, height: extra.height || 100 },
    computed: style({
      'flex-grow': grow,
      'max-width': 'none',
      'max-height': 'none',
      ...extra,
    }),
  };
}

describe('shouldPinFlexGrowItems', () => {
  test('keeps a proportional zero-basis row as flex', () => {
    const container = { width: 1800, height: 300 };
    const computed = style({ 'column-gap': '0px' });
    const children = [item(900, 1.5), item(450, 0.75), item(450, 0.75)];

    expect(shouldPinFlexGrowItems(container, computed, children, 'row')).toBe(false);
  });

  test('pins the whole grow group when padding changes browser flex widths', () => {
    // Regression: `flex: 1.5 / .75 / .75` with 112 / 88 / 88 px horizontal
    // padding produces these measured border-box widths in Chromium. PAGX's
    // zero-basis allocator would instead produce 900 / 450 / 450.
    const container = { width: 1800, height: 300 };
    const computed = style({ 'column-gap': '0px' });
    const children = [
      item(868, 1.5, { 'padding-left': '56px', 'padding-right': '56px' }),
      item(466.5, 0.75, { 'padding-left': '44px', 'padding-right': '44px' }),
      item(465.5, 0.75, { 'padding-left': '44px', 'padding-right': '44px' }),
    ];

    expect(shouldPinFlexGrowItems(container, computed, children, 'row')).toBe(true);
  });

  test('accounts for fixed siblings, container padding, borders, and gap', () => {
    const container = { width: 700, height: 100 };
    const computed = style({
      'padding-left': '20px',
      'padding-right': '20px',
      'border-left-width': '1px',
      'border-right-width': '1px',
      'column-gap': '8px',
    });
    // Inner 658 - fixed 100 - two gaps 16 = 542, split 1:2.
    const children = [
      item(100, 0),
      item(542 / 3, 1),
      item(542 * 2 / 3, 2),
    ];

    expect(shouldPinFlexGrowItems(container, computed, children, 'row')).toBe(false);
  });

  test('pins grow sums below one because PAGX always normalizes the shares', () => {
    const container = { width: 600, height: 100 };
    const computed = style();
    // CSS flex-grow 0.5 consumes half the free space; PAGX would normalize the
    // sole grow item to the whole 600 px.
    const children = [item(300, 0.5)];

    expect(shouldPinFlexGrowItems(container, computed, children, 'row')).toBe(true);
  });

  test('applies the same measured-size guard to column flex layouts', () => {
    const container = { width: 100, height: 600 };
    const computed = style({ 'row-gap': '0px' });
    const children = [
      item(100, 1, { height: 320, 'padding-top': '30px', 'padding-bottom': '30px' }),
      item(100, 1, { height: 280, 'padding-top': '10px', 'padding-bottom': '10px' }),
    ];

    expect(shouldPinFlexGrowItems(container, computed, children, 'column')).toBe(true);
  });

  test('excludes an already-pinned max-capped grow item from PAGX flex space', () => {
    const container = { width: 500, height: 100 };
    const computed = style({ 'column-gap': '0px' });
    const children = [
      item(200, 1, { 'max-width': '200px' }),
      item(300, 1),
    ];

    expect(shouldPinFlexGrowItems(container, computed, children, 'row')).toBe(false);
  });
});
