const { snapshotFontFamily } = require('../dist/lib/browser-snapshot');

test('web aliases preserve the font stack', () => {
  global.window = { __pagxFontAliases: [
    { from: 'Web', to: 'Web Normal', weight: '700' },
    { from: 'Web', to: 'Web Narrow', weight: '900' },
  ] };
  try {
    const values = { 'font-weight': '800', 'font-style': 'normal' };
    expect(snapshotFontFamily('Web, Helvetica', { getPropertyValue: key => values[key] || '' }))
      .toBe("'Web Normal', Helvetica");
  } finally { delete global.window; }
});
