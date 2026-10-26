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
      .toBe("'Web Normal', Helvetica");
  } finally { delete global.window; }
});
