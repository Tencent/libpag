const cases = require('../../../test/fixtures/font-matching-cases.json');
const { fontMatchCompare, snapshotFontFamily } = require('../dist/lib/browser-snapshot');

const props = ([weight, width, style]) => ({ weight: String(weight), stretch: `${width}%`, style });
for (const row of cases) {
  test(row.name, () => {
    const request = props(row.request);
    const faces = row.faces.map(([name, ...values]) => ({ name, ...props(values) }));
    for (let i = 0; i < faces.length; i++) {
      expect([...faces].sort((a, b) => fontMatchCompare(request, a, b))[0].name).toBe(row.expected);
      faces.push(faces.shift());
    }
    expect(faces.reverse().sort((a, b) => fontMatchCompare(request, a, b))[0].name).toBe(row.expected);
  });
}

test('descriptor ranges can match an interior value', () => {
  const request = { weight: '550', stretch: '90%', style: 'normal' };
  expect(fontMatchCompare(request, { weight: '500 600', stretch: '80% 100%' },
    { weight: '550', stretch: '100%' })).toBeLessThan(0);
});

test('web aliases preserve source order, unicode subsets and numeric system requests', () => {
  global.window = { __pagxFontAliases: [
    { from: 'Web', path: 'normal.ttf', weight: '700', stretch: '100%', unicodeRange: 'U+0000-00FF' },
    { from: 'Web', path: 'cjk.ttf', weight: '700', stretch: '100%', unicodeRange: 'U+4E00-9FFF' },
    { from: 'Web', path: 'narrow.ttf', weight: '900', stretch: '75%' },
  ] };
  try {
    const values = { 'font-weight': '800', 'font-stretch': '100%', 'font-style': 'normal' };
    snapshotFontFamily('Web, Helvetica', { getPropertyValue: key => values[key] || '' });
    expect(window.__pagxFontBindings[0].faces.map(f => f.path || f.style))
      .toEqual(['cjk.ttf', 'normal.ttf', '800 100%']);
    expect(window.__pagxFontBindings[0].faces[0].unicodeRange).toBe('U+4E00-9FFF');
  } finally { delete global.window; }
});
