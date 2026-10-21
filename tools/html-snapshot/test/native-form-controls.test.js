// Copyright (C) 2026 Tencent. All rights reserved.

'use strict';

// Browsers paint native form controls (the check tick, radio dot, range
// thumb/track, progress/meter bar, colour swatch, date text) through the UA
// shadow DOM, which the snapshot's light-DOM walk cannot observe — `classify`
// used to drop the element (checkbox/radio/range) or keep a bare box
// (color/date/progress/meter). The `svg*` helpers synthesise the control's
// static appearance as inline SVG; these tests exercise them directly (they are
// pure geometry + string builders) plus `nativeControlKind` routing and the
// password mask in `syntheticText`.

const {
  nativeControlKind, maskPasswordText,
  svgCheckbox, svgRadio, svgRange, svgProgress, svgMeter, svgColorSwatch,
  formatDateControlValue, selectArrowMarkup,
} = require('../dist/lib/browser-snapshot');

function computed(overrides = {}) {
  return { getPropertyValue: (k) => overrides[k] || '' };
}

// Minimal stand-in for an <input>/<progress>/<meter>: only the tag, the
// attribute table and the reflected numeric/boolean properties the helpers read.
function fakeEl(tagName, attrs = {}, props = {}) {
  const table = {};
  for (const [k, v] of Object.entries(attrs)) table[k] = String(v);
  return Object.assign({
    tagName: tagName.toUpperCase(),
    getAttribute: (k) => (k in table ? table[k] : null),
  }, props);
}

describe('nativeControlKind', () => {
  test('routes the replaced controls', () => {
    expect(nativeControlKind(fakeEl('input', { type: 'checkbox' }))).toBe('checkbox');
    expect(nativeControlKind(fakeEl('input', { type: 'radio' }))).toBe('radio');
    expect(nativeControlKind(fakeEl('input', { type: 'range' }))).toBe('range');
    expect(nativeControlKind(fakeEl('input', { type: 'color' }))).toBe('color');
    expect(nativeControlKind(fakeEl('input', { type: 'date' }))).toBe('date');
    expect(nativeControlKind(fakeEl('progress'))).toBe('progress');
    expect(nativeControlKind(fakeEl('meter'))).toBe('meter');
  });

  test('does not synthesize hidden or custom-painted controls', () => {
    const el = fakeEl('input', { type: 'checkbox' });
    expect(nativeControlKind(el, { ...computed(), display: 'none' })).toBeNull();
    expect(nativeControlKind(el, { ...computed(), visibility: 'hidden' })).toBeNull();
    expect(nativeControlKind(el, computed({ appearance: 'none' }))).toBeNull();
    expect(nativeControlKind(el, computed({ '-webkit-appearance': 'none' }))).toBeNull();
  });

  test('leaves text inputs and non-controls to the normal pipeline', () => {
    expect(nativeControlKind(fakeEl('input', { type: 'text' }))).toBeNull();
    expect(nativeControlKind(fakeEl('input', { type: 'password' }))).toBeNull();
    expect(nativeControlKind(fakeEl('input'))).toBeNull();
    expect(nativeControlKind(fakeEl('div'))).toBeNull();
    expect(nativeControlKind(null)).toBeNull();
  });
});

describe('maskPasswordText', () => {
  test('emits one bullet per character', () => {
    expect(maskPasswordText('secret')).toBe('\u2022\u2022\u2022\u2022\u2022\u2022');
  });

  test('is empty for an empty value', () => {
    expect(maskPasswordText('')).toBe('');
  });
});

describe('svgCheckbox', () => {
  test('checked: accent fill with a white tick', () => {
    const el = fakeEl('input', { type: 'checkbox' }, { checked: true });
    const svg = svgCheckbox(el, computed({ 'accent-color': 'rgb(22, 93, 255)' }), 16, 16);
    expect(svg).toContain('fill="rgb(22, 93, 255)"');
    expect(svg).toContain('<path');
    expect(svg).toContain('stroke="#FFFFFF"');
    expect(svg).toContain('width="16px"');
  });

  test('unchecked: white square with the UA grey border and no tick', () => {
    const el = fakeEl('input', { type: 'checkbox' }, { checked: false });
    const svg = svgCheckbox(el, computed(), 16, 16);
    expect(svg).toContain('stroke="#767676"');
    expect(svg).not.toContain('<path');
  });
});

describe('svgRadio', () => {
  test('checked: accent ring with an inner accent dot', () => {
    const el = fakeEl('input', { type: 'radio' }, { checked: true });
    const svg = svgRadio(el, computed({ 'accent-color': 'rgb(22, 93, 255)' }), 16, 16);
    expect(svg.match(/<circle/g)).toHaveLength(2);
    expect(svg).toContain('fill="rgb(22, 93, 255)"');
  });

  test('unchecked: a single grey ring', () => {
    const el = fakeEl('input', { type: 'radio' }, { checked: false });
    const svg = svgRadio(el, computed(), 16, 16);
    expect(svg.match(/<circle/g)).toHaveLength(1);
    expect(svg).toContain('stroke="#767676"');
  });
});

describe('svgRange', () => {
  test('fills to the value fraction and places the thumb on the fill edge', () => {
    const el = fakeEl('input', { type: 'range' }, { value: '64', min: '0', max: '100' });
    const svg = svgRange(el, computed({ 'accent-color': 'rgb(22, 93, 255)' }), 468, 16);
    expect(svg).toContain('fill="rgb(22, 93, 255)"');
    // thumb centre = thumb radius + (width − 2·radius) · fraction = 8 + 452·0.64
    expect(svg).toContain('cx="297.3"');
  });
});

describe('svgProgress / svgMeter', () => {
  test('progress fills by value/max with the UA blue', () => {
    const el = fakeEl('progress', {}, { value: 72, min: 0, max: 100 });
    const svg = svgProgress(el, computed(), 400, 10);
    expect(svg).toContain('width="288"'); // 400 · 0.72
    expect(svg).toContain('rgb(0, 117, 255)');
  });

  test('meter picks the suboptimal colour inside the low/high band', () => {
    const el = fakeEl('meter', {}, { value: 0.35, min: 0, max: 1, low: 0.3, high: 0.7, optimum: 0.9 });
    const svg = svgMeter(el, computed(), 400, 10);
    expect(svg).toContain('rgb(255, 185, 0)');
  });
});

describe('svgColorSwatch', () => {
  test('fills the swatch with the input value', () => {
    const el = fakeEl('input', { type: 'color' }, { value: '#5b4be0' });
    const svg = svgColorSwatch(el, 44, 32);
    expect(svg).toContain('fill="#5b4be0"');
    // canonical 44×32 box ⇒ 32×16 swatch
    expect(svg).toContain('width="32px"');
  });
});

describe('formatDateControlValue', () => {
  test('renders the ISO date-year for a date input', () => {
    expect(formatDateControlValue('2026-10-08', 'date')).toMatch(/2026/);
  });

  test('passes through empty and unparseable values', () => {
    expect(formatDateControlValue('', 'date')).toBe('');
    expect(formatDateControlValue('nonsense', 'date')).toBe('nonsense');
  });
});

describe('selectArrowMarkup', () => {
  // The sample form's control: 469.5x36 box, `padding: 0 12px`, 1px border.
  const controlComputed = computed({
    color: 'rgb(29, 33, 41)',
    'padding-left': '12px',
    'padding-top': '0px',
    'border-left-width': '1px',
    'border-left-style': 'solid',
    'border-top-width': '1px',
    'border-top-style': 'solid',
  });

  test('lands the chevron inside the trailing edge, backing out border + padding', () => {
    const svg = selectArrowMarkup(controlComputed, { width: 469.5, height: 36 });
    // Target svg left edge: width - 10 (chevron) - 3 (trailing gap) = 456.5; the importer
    // anchors the inset at the padded inner host's origin, so border + padding come off.
    expect(svg).toContain('left:443.5px');
    expect(svg).toContain('top:14px');
    expect(svg).toContain('width="10" height="6"');
    expect(svg).toContain('stroke="rgb(29, 33, 41)"');
    expect(svg).toContain('d="M1 1 L5 5 L9 1"');
  });

  test('falls back to the control text colour when none is computed', () => {
    const svg = selectArrowMarkup(computed(), { width: 100, height: 36 });
    expect(svg).toContain('stroke="rgb(29, 33, 41)"');
  });
});
