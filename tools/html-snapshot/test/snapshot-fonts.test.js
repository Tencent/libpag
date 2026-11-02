// Copyright (C) 2026 Tencent. All rights reserved.
'use strict';
const { collectSnapshotFontMetadata, prepareSnapshotFontReplacements } = require('../dist/lib/snapshot-fonts');
const source = { families: ['Source & Co', 'sans-serif'], weight: 650, style: 'normal', lang: 'zh' };
const style = "font-family:'Source &amp; Co',sans-serif;font-weight:650";
const fonts = [{ fontName: { family: 'Source & Co', style: 'Medium Italic' } }];

test('collects standard inherited CSS, exact numeric weights and language without request attributes', () => {
  const html = `<html lang="zh"><head><style>ignored</style></head><body>
    <span style="${style}">A<b style="font-weight:700">B</b></span>
    <span style="${style}">C</span><svg><text>missing</text></svg></body></html>`;
  expect(collectSnapshotFontMetadata(html)).toEqual({ version: 1, fonts: [
    { ...source, weightName: '650' }, { ...source, weight: 700, weightName: 'Bold' },
  ], missingRequests: 1 });
});

test('reads variation axes from computed HTML styles', () => {
  const result = collectSnapshotFontMetadata(`<div lang="ar"><span
    style="font-family:Arabic;font-weight:500;font-style:oblique 14deg;font-variation-settings:'wght' 550,'wdth' 90">Hi</span></div>`);
  expect(result.fonts).toEqual([{ families: ['Arabic'], weight: 500, weightName: 'Medium',
    style: 'oblique', variations: { wght: 550, wdth: 90 }, lang: 'ar' }]);
});

test('does not guess SVG computed requests from retained authored markup', () => {
  const html = `<div style="font-family:Source"><svg><text font-family="Arabic">Hi</text></svg></div>`;
  expect(collectSnapshotFontMetadata(html)).toEqual({ version: 1, fonts: [], missingRequests: 1 });
  expect(prepareSnapshotFontReplacements(html, fonts).mapping)
    .toEqual({ replaced: 0, unmatched: 0, missingRequests: 1 });
});

test('prepares exact target faces without altering source CSS or inheriting a mapped parent', () => {
  const html = `<div lang="zh" style="${style};left:12px;color:red">A<span>B</span>
    <span style="font-weight:700">C</span></div>`;
  const mapped = prepareSnapshotFontReplacements(html, fonts);
  expect(mapped.mapping).toEqual({ replaced: 3, unmatched: 0, missingRequests: 0 });
  expect(mapped.replacements).toEqual([{
    families: "'Source & Co',sans-serif", weight: 650, style: 'normal',
    targetFamily: 'Source & Co', targetStyle: 'Medium Italic',
  }, {
    families: "'Source & Co',sans-serif", weight: 700, style: 'normal',
    targetFamily: 'Source & Co', targetStyle: 'Medium Italic',
  }]);
  expect(collectSnapshotFontMetadata(html).fonts[0].families).toEqual(source.families);
  expect(prepareSnapshotFontReplacements(html, []).replacements).toEqual([]);
});

test('parses semicolons in quoted family names and ignores unrelated URL styles', () => {
  const html = `<span style="font-family:'Source; Co';font-weight:650;background-image:url('a;b.png')">A</span>`;
  const custom = { fontName: { family: 'Source; Co', style: 'Regular' } };
  const result = prepareSnapshotFontReplacements(html, [custom]);
  expect(result.mapping.replaced).toBe(1);
  expect(result.replacements[0].families).toBe("'Source; Co'");
});


// A snapshot carries the page's `@font-face` rules in its single <style> block.
// The descriptors on those rules decide which physical cut the browser painted
// with, so the catalog match has to start from that cut — while the
// `--font-replacement` key stays the computed request the subset still declares.
function face(family, weight, style, src) {
  return '@font-face{font-family:' + family + ';src:url(' +
    JSON.stringify(src || 'data:font/woff2;base64,AAAA') + ');font-weight:' + weight +
    ';font-style:' + style + '}';
}

function page(faces, body) {
  return '<html><head><style>' + faces.join('') + '</style></head><body>' + body + '</body></html>';
}

describe('source @font-face resolution', () => {
  test('matches the declared 700 cut for a computed 400 request without changing the key', () => {
    const html = page(
      [face('"Cinzel Decorative"', '700', 'normal')],
      '<div style="font-family:\'Cinzel Decorative\';font-size:40px">IMPERIUM</div>',
    );
    const catalog = [
      { fontName: { family: 'Cinzel Decorative', style: 'Regular' } },
      { fontName: { family: 'Cinzel Decorative', style: 'Bold' } },
    ];
    expect(prepareSnapshotFontReplacements(html, catalog).replacements).toEqual([{
      families: "'Cinzel Decorative'", weight: 400, style: 'normal',
      targetFamily: 'Cinzel Decorative', targetStyle: 'Bold',
    }]);
    expect(collectSnapshotFontMetadata(html).fonts).toEqual([{
      families: ['Cinzel Decorative'], weight: 400, weightName: 'Regular', style: 'normal',
    }]);
  });

  test('keeps requests that already sit inside a variable face range', () => {
    const html = page(
      [
        face('"Playfair"', '400 900', 'normal'),
        face('"Playfair"', '400 900', 'italic', 'data:font/woff2;base64,BBBB'),
      ],
      '<span style="font-family:\'Playfair\'">A</span>' +
      '<span style="font-family:\'Playfair\';font-weight:900">B</span>' +
      '<span style="font-family:\'Playfair\';font-style:italic">C</span>',
    );
    const catalog = [
      { fontName: { family: 'Playfair', style: 'Regular' } },
      { fontName: { family: 'Playfair', style: 'Bold' } },
      { fontName: { family: 'Playfair', style: 'Black' } },
      { fontName: { family: 'Playfair', style: 'Italic' } },
    ];
    expect(prepareSnapshotFontReplacements(html, catalog).replacements
      .map((item) => [item.weight, item.style, item.targetStyle])).toEqual([
      [400, 'normal', 'Regular'],
      [900, 'normal', 'Black'],
      [400, 'italic', 'Italic'],
    ]);
  });

  test('reads descriptors that follow a data: source with embedded semicolons', () => {
    const html = page(
      ['@font-face{font-family:"Inline";src:url("data:font/woff2;base64,AA;BB") format("woff2");' +
        'font-weight:bold;font-style:normal}'],
      '<span style="font-family:Inline">A</span>',
    );
    expect(prepareSnapshotFontReplacements(html, [
      { fontName: { family: 'Inline', style: 'Regular' } },
      { fontName: { family: 'Inline', style: 'Bold' } },
    ]).replacements[0].targetStyle).toBe('Bold');
  });

  test('resolves inherited family requests from the inherited run', () => {
    const html = page(
      [face('Brand', '700', 'normal')],
      '<div style="font-family:Brand"><span style="font-size:20px">A</span>' +
      '<span style="font-size:20px">B</span></div>',
    );
    const mapped = prepareSnapshotFontReplacements(html, [
      { fontName: { family: 'Brand', style: 'Regular' } },
      { fontName: { family: 'Brand', style: 'Bold' } },
    ]);
    expect(mapped.replacements).toEqual([{
      families: 'Brand', weight: 400, style: 'normal',
      targetFamily: 'Brand', targetStyle: 'Bold',
    }]);
    expect(mapped.mapping).toEqual({ replaced: 2, unmatched: 0, missingRequests: 0 });
  });

  test('decodes CSS escapes in a declared family name', () => {
    const html = page(
      ['@font-face{font-family:"A \\26  B";src:url("data:font/woff2;base64,AA");' +
        'font-weight:700;font-style:normal}'],
      '<span style="font-family:\'A &amp; B\'">A</span>',
    );
    const mapped = prepareSnapshotFontReplacements(html, [
      { fontName: { family: 'A & B', style: 'Regular' } },
      { fontName: { family: 'A & B', style: 'Bold' } },
    ]);
    expect(mapped.replacements[0]).toMatchObject({
      families: "'A & B'", targetFamily: 'A & B', targetStyle: 'Bold',
    });
  });

  test('keeps the authored family while the target family differs', () => {
    const html = page([face('XiaoWei', '700', 'normal')],
      '<span style="font-family:XiaoWei">A</span>');
    expect(prepareSnapshotFontReplacements(html, [
      { fontName: { family: 'ZCOOL XiaoWei', style: 'Regular' } },
      { fontName: { family: 'ZCOOL XiaoWei', style: 'Bold' } },
    ]).replacements).toEqual([{
      families: 'XiaoWei', weight: 400, style: 'normal',
      targetFamily: 'ZCOOL XiaoWei', targetStyle: 'Bold',
    }]);
  });

  test('resolves only the head of the family stack', () => {
    const html = page([face('First', '700', 'normal'), face('Second', '700', 'normal')],
      '<span style="font-family:\'First\',\'Second\'">A</span>' +
      '<span style="font-family:\'Arial\',\'Second\'">B</span>');
    const mapped = prepareSnapshotFontReplacements(html, [
      { fontName: { family: 'First', style: 'Regular' } },
      { fontName: { family: 'First', style: 'Bold' } },
      { fontName: { family: 'Second', style: 'Regular' } },
      { fontName: { family: 'Second', style: 'Bold' } },
      { fontName: { family: 'Arial', style: 'Regular' } },
      { fontName: { family: 'Arial', style: 'Bold' } },
    ]);
    expect(mapped.replacements.map((item) => [item.families, item.targetFamily, item.targetStyle]))
      .toEqual([
        ["'First','Second'", 'First', 'Bold'],
        // Arial may be installed on the machine, so Second's declared 700 must not make the
        // Arial request bold.
        ["'Arial','Second'", 'Arial', 'Regular'],
      ]);
  });

  test('leaves the request alone when the stack head is a generic family', () => {
    const html = page([face('Second', '700', 'normal')],
      '<span style="font-family:serif,\'Second\'">A</span>');
    expect(prepareSnapshotFontReplacements(html, [
      { fontName: { family: 'Second', style: 'Regular' } },
      { fontName: { family: 'Second', style: 'Bold' } },
    ]).replacements[0]).toMatchObject({ weight: 400, targetStyle: 'Regular' });
  });

  test('takes both descriptors from a single bold italic face', () => {
    const html = page([face('Fancy', '700', 'italic')],
      '<span style="font-family:Fancy">A</span>');
    expect(prepareSnapshotFontReplacements(html, [
      { fontName: { family: 'Fancy', style: 'Regular' } },
      { fontName: { family: 'Fancy', style: 'Italic' } },
      { fontName: { family: 'Fancy', style: 'Bold' } },
      { fontName: { family: 'Fancy', style: 'Bold Italic' } },
    ]).replacements[0]).toMatchObject({
      weight: 400, style: 'normal', targetStyle: 'Bold Italic',
    });
  });

  test('keeps a heavier computed weight when the page declares only a lighter face', () => {
    const html = page([face('Space Mono', '400', 'normal')],
      '<span style="font-family:\'Space Mono\';font-weight:700">A</span>');
    expect(prepareSnapshotFontReplacements(html, [
      { fontName: { family: 'Space Mono', style: 'Regular' } },
      { fontName: { family: 'Space Mono', style: 'Bold' } },
    ]).replacements[0]).toMatchObject({ weight: 700, targetStyle: 'Bold' });
  });

  test('matches on a fractional declared weight without rounding it first', () => {
    const html = page([face('Half', '450.5', 'normal')],
      '<span style="font-family:Half">A</span>');
    expect(prepareSnapshotFontReplacements(html, [
      { fontName: { family: 'Half', style: 'Regular' } },
      { fontName: { family: 'Half', style: 'Medium' } },
    ]).replacements[0]).toMatchObject({ weight: 400, targetStyle: 'Medium' });
  });

  test('keeps the requested slant when the family declares no slanted face', () => {
    // An italic request answered by an upright face is synthesised by the browser; PAGX
    // cannot synthesise, so the catalog still has to resolve to a slanted face.
    const html = page([face('Upright', '400', 'normal')],
      '<span style="font-family:Upright;font-style:italic">A</span>');
    expect(prepareSnapshotFontReplacements(html, [
      { fontName: { family: 'Upright', style: 'Regular' } },
      { fontName: { family: 'Upright', style: 'Italic' } },
    ]).replacements[0]).toMatchObject({
      weight: 400, style: 'italic', targetStyle: 'Italic',
    });
  });

  test('reads a quoted family with braces and a data: source with semicolons', () => {
    const html = page(
      ['@font-face{font-family:"Bra{nd}";src:url("data:font/woff2;base64,AA;BB") format("woff2");' +
        'font-weight:700;font-style:normal}'],
      '<span style="font-family:\'Bra{nd}\'">A</span>',
    );
    expect(prepareSnapshotFontReplacements(html, [
      { fontName: { family: 'Bra{nd}', style: 'Regular' } },
      { fontName: { family: 'Bra{nd}', style: 'Bold' } },
    ]).replacements[0]).toMatchObject({
      families: "'Bra{nd}'", weight: 400, targetStyle: 'Bold',
    });
  });

  test('matches the declared italic cut for an upright request', () => {
    const html = page([face('Slanted', '400', 'italic')],
      '<span style="font-family:Slanted">A</span>');
    expect(prepareSnapshotFontReplacements(html, [
      { fontName: { family: 'Slanted', style: 'Regular' } },
      { fontName: { family: 'Slanted', style: 'Italic' } },
    ]).replacements[0]).toMatchObject({
      weight: 400, style: 'normal', targetStyle: 'Italic',
    });
  });

  test('keeps the computed request when a face declares no descriptor or an invalid one', () => {
    const html = page(
      [
        '@font-face{font-family:Plain;src:url("data:font/woff2;base64,AA")}',
        '@font-face{font-family:Odd;src:url("data:font/woff2;base64,BB");font-weight:bolder}',
      ],
      '<span style="font-family:Plain">A</span><span style="font-family:Odd">B</span>',
    );
    const catalog = [
      { fontName: { family: 'Plain', style: 'Regular' } },
      { fontName: { family: 'Plain', style: 'Bold' } },
      { fontName: { family: 'Odd', style: 'Regular' } },
      { fontName: { family: 'Odd', style: 'Bold' } },
    ];
    expect(prepareSnapshotFontReplacements(html, catalog).replacements
      .map((item) => [item.weight, item.targetStyle])).toEqual([
      [400, 'Regular'],
      [400, 'Regular'],
    ]);
  });

  test('keeps a comment marker inside a quoted family name', () => {
    const html = page(
      ['@font-face{font-family:"A/*B*/C";src:url("data:font/woff2;base64,AA");' +
        'font-weight:700;font-style:normal}'],
      '<span style="font-family:\'A/*B*/C\'">A</span>',
    );
    expect(prepareSnapshotFontReplacements(html, [
      { fontName: { family: 'A/*B*/C', style: 'Regular' } },
      { fontName: { family: 'A/*B*/C', style: 'Bold' } },
    ]).replacements[0]).toMatchObject({
      families: "'A/*B*/C'", weight: 400, targetStyle: 'Bold',
    });
  });

  test('ignores an @font-face token inside a quoted value or a comment', () => {
    const html = page(
      [
        '.x::before{content:"@font-face{font-family:Fake;src:url(fake.woff2);font-weight:700}"}',
        '/* @font-face{font-family:Commented;src:url(c.woff2);font-weight:700} */',
      ],
      '<span style="font-family:Fake">A</span><span style="font-family:Commented">B</span>',
    );
    const catalog = ['Fake', 'Commented'].flatMap((family) => ['Regular', 'Bold']
      .map((style) => ({ fontName: { family, style } })));
    expect(prepareSnapshotFontReplacements(html, catalog).replacements
      .map((item) => [item.families, item.targetStyle])).toEqual([
      ['Fake', 'Regular'],
      ['Commented', 'Regular'],
    ]);
  });

  test('reads a source whose data: URI contains braces', () => {
    const html = page(
      [
        '@font-face{font-family:"Quoted";src:url("data:font/woff2;base64,AA{BB");' +
          'font-weight:700;font-style:normal}',
        '@font-face{font-family:Bare;src:url(data:font/woff2;base64,CC{DD);' +
          'font-weight:700;font-style:normal}',
      ],
      '<span style="font-family:Quoted">A</span><span style="font-family:Bare">B</span>',
    );
    const catalog = ['Quoted', 'Bare'].flatMap((family) => ['Regular', 'Bold']
      .map((style) => ({ fontName: { family, style } })));
    expect(prepareSnapshotFontReplacements(html, catalog).replacements
      .map((item) => [item.families, item.targetStyle])).toEqual([
      ['Quoted', 'Bold'],
      ['Bare', 'Bold'],
    ]);
  });

  test('ignores a rule without a family or a source', () => {
    const html = page(
      [
        '@font-face{src:url("data:font/woff2;base64,AA");font-weight:700}',
        '@font-face{font-family:NoSource;font-weight:700}',
      ],
      '<span style="font-family:NoSource">A</span>',
    );
    expect(prepareSnapshotFontReplacements(html, [
      { fontName: { family: 'NoSource', style: 'Regular' } },
      { fontName: { family: 'NoSource', style: 'Bold' } },
    ]).replacements[0].targetStyle).toBe('Regular');
  });
});
