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

test('web aliases match CSS width before weight and preserve the font stack', () => {
  global.window = { __pagxFontAliases: [
    { from: 'Web', to: 'Web Normal', weight: '700', stretch: '100%' },
    { from: 'Web', to: 'Web Narrow', weight: '900', stretch: '75%' },
  ] };
  try {
    const values = { 'font-weight': '800', 'font-stretch': '100%', 'font-style': 'normal' };
    expect(snapshotFontFamily('Web, Helvetica', { getPropertyValue: key => values[key] || '' }))
      .toBe("Web, 'Web Normal', Helvetica");
  } finally { delete global.window; }
});


test('matches the CSS family before the internal name and retains the declared cut', () => {
  global.window = { __pagxFontAliases: [
    { from: 'Playfair', to: 'Playfair Display', weight: '400 900', style: 'italic' },
  ] };
  try {
    const { prepareSnapshotFontReplacements } = require('../dist/lib/snapshot-fonts');
    const values = { 'font-weight': '500', 'font-style': 'italic', 'font-stretch': '100%' };
    const family = snapshotFontFamily('Playfair, serif', { getPropertyValue: key => values[key] || '' });
    expect(family).toBe("Playfair, 'Playfair Display', serif");
    const result = prepareSnapshotFontReplacements(
      `<span style="font-family:${family};font-weight:500;font-style:italic">Ag</span>`,
      ['Playfair', 'Playfair Display'].map(family => ({ fontName: { family, style: 'Medium Italic' } })),
    );
    expect(result.replacements).toEqual([{
      families: family, weight: 500, style: 'italic', targetFamily: 'Playfair', targetStyle: 'Medium Italic',
    }]);
    window.__pagxFontAliases[0].to = 'Playfair';
    expect(snapshotFontFamily('Playfair, serif', { getPropertyValue: key => values[key] || '' }))
      .toBe('Playfair, serif');
  } finally { delete global.window; }
});
