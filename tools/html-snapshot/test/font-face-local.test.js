// Copyright (C) 2026 Tencent. All rights reserved.

'use strict';

// Regression tests for the snapshot's web-font carry-over. The subset rebuilds
// the page with inline styles only, so without these rules the importer never
// learns which font a `font-family` name stands for, and a design export whose
// text lives in `assets/fonts/*.woff2` (or inside `data:` URIs) renders with a
// substitute face. `collectCarryOverFontFaces` reads the page's `@font-face`
// rules and `cssQuotedString` writes them back so both the browser and the
// importer's XML + CSS readers decode the same strings.
//
// The weight and style descriptors travel with the source: a family may be
// declared with a single 700 face while the page's CSS asks for 400, and the
// browser then paints the 700 outlines. Without the descriptor the subset (and
// the editor-catalog match derived from it) would claim the regular cut.

const { collectCarryOverFontFaces, cssQuotedString } = require('../dist/lib/browser-snapshot');
const {
  fontFaceRenderedWeight, fontFaceRuleCss, fontFaceWeightRange,
  normalizeFontFaceStyle, normalizeFontFaceWeight,
} = require('../dist/lib/font-face-pass');

function fontFaceRule(family, src, descriptors) {
  const extra = descriptors || {};
  return {
    type: 5,
    style: {
      getPropertyValue: (name) => {
        if (name === 'font-family') return family;
        if (name === 'src') return src;
        if (name === 'font-weight') return extra.weight || '';
        if (name === 'font-style') return extra.style || '';
        return '';
      },
    },
  };
}

function sheet(rules, href) {
  return { href: href || null, cssRules: rules };
}

const PAGE = 'file:///tmp/export/index.html';

describe('collectCarryOverFontFaces', () => {
  test('keeps the first local url() source resolved against the page', () => {
    const faces = collectCarryOverFontFaces([
      sheet([
        fontFaceRule(
          '"pagx-font-f1"',
          'local("Missing"), url("https://cdn.example/f1.woff2") format("woff2"), ' +
            'url("assets/fonts/font_f1.woff2") format("woff2"), url(font_f1.ttf)',
        ),
      ]),
    ], PAGE);
    expect(faces).toEqual([{
      family: 'pagx-font-f1',
      src: '/tmp/export/assets/fonts/font_f1.woff2',
      weight: '400',
      style: 'normal',
    }]);
  });

  test('keeps a data: source verbatim and drops remote or name-only sources', () => {
    const faces = collectCarryOverFontFaces([
      sheet([
        fontFaceRule('Remote', 'url(https://cdn.example/remote.woff2)'),
        fontFaceRule('Inline', 'url("data:font/woff2;base64,AAAA") format("woff2")'),
        fontFaceRule('Named', 'local("Arial")'),
      ]),
    ], PAGE);
    expect(faces).toEqual([{
      family: 'Inline', src: 'data:font/woff2;base64,AAAA', weight: '400', style: 'normal',
    }]);
  });

  test('resolves against the stylesheet URL and percent-decodes the path', () => {
    const faces = collectCarryOverFontFaces([
      sheet([fontFaceRule("'My Font'", "url('../fonts/My%20Font.woff2')")],
        'file:///tmp/export/css/site.css'),
    ], PAGE);
    expect(faces).toEqual([{
      family: 'My Font', src: '/tmp/export/fonts/My Font.woff2', weight: '400', style: 'normal',
    }]);
  });

  test('visits grouping rules and imported sheets, and skips inaccessible sheets', () => {
    const imported = sheet([fontFaceRule('Imported', 'url(imported.woff2)')],
      'file:///tmp/export/css/imported.css');
    const inaccessible = {
      href: 'https://cdn.example/fonts.css',
      get cssRules() {
        throw new Error('SecurityError');
      },
    };
    const faces = collectCarryOverFontFaces([
      inaccessible,
      sheet([
        { styleSheet: imported },
        { type: 4, cssRules: [fontFaceRule('Media', 'url(media.woff2)')] },
      ]),
    ], PAGE);
    expect(faces).toEqual([
      { family: 'Imported', src: '/tmp/export/css/imported.woff2', weight: '400', style: 'normal' },
      { family: 'Media', src: '/tmp/export/media.woff2', weight: '400', style: 'normal' },
    ]);
  });

  test('reports each (family, source, descriptors) combination once', () => {
    const rule = fontFaceRule('Brand', 'url(brand.woff2)');
    const faces = collectCarryOverFontFaces([sheet([rule, rule])], PAGE);
    expect(faces).toEqual([{
      family: 'Brand', src: '/tmp/export/brand.woff2', weight: '400', style: 'normal',
    }]);
  });

  test('keeps one entry per declared cut instead of collapsing a family to one source', () => {
    const faces = collectCarryOverFontFaces([
      sheet([
        fontFaceRule('Brand', 'url(brand-regular.woff2)', { weight: '400' }),
        fontFaceRule('Brand', 'url(brand-bold.woff2)', { weight: '700' }),
        fontFaceRule('Brand', 'url(brand-italic.woff2)', { weight: '400', style: 'italic' }),
      ]),
    ], PAGE);
    expect(faces.map((face) => [face.weight, face.style, face.src])).toEqual([
      ['400', 'normal', '/tmp/export/brand-regular.woff2'],
      ['700', 'normal', '/tmp/export/brand-bold.woff2'],
      ['400', 'italic', '/tmp/export/brand-italic.woff2'],
    ]);
  });

  test('keeps faces that share a source but differ in a descriptor', () => {
    const faces = collectCarryOverFontFaces([
      sheet([
        fontFaceRule('Variable', 'url(variable.woff2)', { weight: '400 900' }),
        fontFaceRule('Variable', 'url(variable.woff2)', { weight: '400 900', style: 'italic' }),
      ]),
    ], PAGE);
    expect(faces.map((face) => [face.weight, face.style])).toEqual([
      ['400 900', 'normal'],
      ['400 900', 'italic'],
    ]);
  });

  test('normalises keyword, numeric and range weights and slants', () => {
    const faces = collectCarryOverFontFaces([
      sheet([
        fontFaceRule('A', 'url(a.woff2)', { weight: 'bold', style: 'normal' }),
        fontFaceRule('B', 'url(b.woff2)', { weight: 'normal', style: 'oblique 14deg' }),
        fontFaceRule('C', 'url(c.woff2)', { weight: '400 900', style: 'italic' }),
        fontFaceRule('D', 'url(d.woff2)', { weight: 'bolder', style: 'inherit' }),
      ]),
    ], PAGE);
    expect(faces.map((face) => [face.weight, face.style])).toEqual([
      ['700', 'normal'],
      ['400', 'oblique'],
      ['400 900', 'italic'],
      ['400', 'normal'],
    ]);
  });

  test('tolerates a missing stylesheet list', () => {
    expect(collectCarryOverFontFaces(null, PAGE)).toEqual([]);
  });
});

describe('fontFaceRuleCss', () => {
  test('writes the source together with the descriptors that select it', () => {
    expect(fontFaceRuleCss({
      family: 'Cinzel Decorative',
      src: 'data:font/woff2;base64,AA',
      weight: '700',
      style: 'normal',
    })).toBe('@font-face{font-family:"Cinzel Decorative"' +
      ';src:url("data:font/woff2;base64,AA");font-weight:700;font-style:normal}');
  });

  test('keeps a variable weight range and an italic cut distinct', () => {
    expect(fontFaceRuleCss({
      family: 'Playfair', src: '/tmp/export/playfair-italic.woff2',
      weight: '400 900', style: 'italic',
    })).toBe('@font-face{font-family:"Playfair"' +
      ';src:url("/tmp/export/playfair-italic.woff2");font-weight:400 900;font-style:italic}');
  });
});

describe('cssQuotedString', () => {
  test('passes plain text through inside double quotes', () => {
    expect(cssQuotedString('/tmp/export/assets/fonts/font_f1.woff2'))
      .toBe('"/tmp/export/assets/fonts/font_f1.woff2"');
  });

  test('writes quotes, backslashes and XML-special characters as CSS hex escapes', () => {
    expect(cssQuotedString('a"b\\c<d>e&f\ng')).toBe('"a\\22 b\\5c c\\3c d\\3e e\\26 f\\a g"');
  });
});

// The helpers reach the page through `Function.prototype.toString()`, which keeps a body
// verbatim: a name an `import` compiled to (`module_1.helper`) does not exist in the payload
// scope and fails at runtime, not at build time. Evaluating the payload source here proves
// every helper the serialised bodies call is actually declared.
describe('browser payload', () => {
  function evaluate(source) {
    return new Function(source + '\nreturn arguments.callee === undefined;')();
  }

  test('declares the face helpers the snapshot payload calls', () => {
    const { HELPERS_SRC } = require('../dist/lib/browser-snapshot');
    expect(() => evaluate(HELPERS_SRC)).not.toThrow();
    const probe = new Function(HELPERS_SRC +
      '\nreturn [typeof collectCarryOverFontFaces, typeof fontFaceRuleCss,' +
      ' typeof normalizeFontFaceWeight, typeof normalizeFontFaceStyle];');
    expect(probe()).toEqual(['function', 'function', 'function', 'function']);
  });

  test('declares the face helpers the icon-font payload calls', () => {
    const { ICON_FONT_HELPERS_SRC } = require('../dist/lib/icon-font');
    const probe = new Function(ICON_FONT_HELPERS_SRC +
      '\nreturn [typeof selectFontFace, typeof fontWeightPreference,' +
      ' typeof FONT_FACE_HELPERS.selectFontFace, typeof FONT_FACE_HELPERS.fontWeightPreference];');
    expect(probe()).toEqual(['function', 'function', 'function', 'function']);
  });
});

describe('face descriptors', () => {
  test('accepts only weights inside the CSS range and keeps a range ordered', () => {
    expect(['1', '400', '1000', '400 900', 'bold', 'normal', '0', '1001', '900 100',
      '400 1001', 'abc', '', '400.5', '400.5 900.9', '0.5', '400.9 400.1', '1000.5', '450.5',
      '9e2'].map(normalizeFontFaceWeight)).toEqual([
      '1', '400', '1000', '400 900', '700', '400', '400', '400', '400', '400', '400', '400',
      '400.5', '400.5 900.9', '400', '400', '400', '450.5', '900',
    ]);
  });

  test('accepts only the slant keywords and an oblique angle inside the CSS range', () => {
    expect(['normal', 'italic', 'oblique', 'oblique 14deg', 'oblique -14deg', 'oblique 120deg',
      'italicgarbage', 'obliquex', ''].map(normalizeFontFaceStyle)).toEqual([
      'normal', 'italic', 'oblique', 'oblique', 'oblique', 'normal',
      'normal', 'normal', 'normal',
    ]);
  });

  test('reads a unit-bearing oblique angle inside -90..90 and rejects the rest', () => {
    expect(['oblique 0.1turn', 'oblique 100grad', 'oblique 1.5707963rad', 'oblique 90deg',
      'oblique 0', 'oblique 0.3turn', 'oblique 1.6rad', 'oblique 1turn', 'oblique 1e3deg',
      'oblique infdeg', 'oblique 14', 'oblique nandeg'].map(normalizeFontFaceStyle)).toEqual([
      'oblique', 'oblique', 'oblique', 'oblique',
      'normal', 'normal', 'normal', 'normal', 'normal',
      'normal', 'normal', 'normal',
    ]);
  });

  test('keeps a fractional endpoint until the face is selected', () => {
    const face = { weight: '450.5 900.5' };
    expect(fontFaceWeightRange(face)).toEqual([450.5, 900.5]);
    expect(fontFaceRenderedWeight(face, 400)).toBe(450.5);
    expect(fontFaceRenderedWeight(face, 700)).toBe(700);
    expect(fontFaceRenderedWeight(face, 1000)).toBe(1000);
  });

  test('only promotes a request to a heavier cut and never lowers it', () => {
    expect(fontFaceRenderedWeight({ weight: '400' }, 700)).toBe(700);
    expect(fontFaceRenderedWeight({ weight: '300' }, 400)).toBe(400);
    expect(fontFaceRenderedWeight({ weight: '700' }, 400)).toBe(700);
    expect(fontFaceRenderedWeight({ weight: '400 900' }, 700)).toBe(700);
    expect(fontFaceRenderedWeight({ weight: '400 900' }, 300)).toBe(400);
  });
});


test('optionally carries HTTP descriptors for matching without downloading font bytes', () => {
  const faces = collectCarryOverFontFaces([sheet([
    fontFaceRule('Remote', 'url(../fonts/bold.woff2)', { weight: '700' }),
    fontFaceRule('Variable', 'url(https://cdn.example/italic.woff2)',
      { weight: '400 900', style: 'italic' }),
    fontFaceRule('Unsupported', 'url(ftp://cdn.example/font.ttf)'),
  ], 'https://example.com/css/fonts.css')], PAGE, true);
  expect(faces).toEqual([
    { family: 'Remote', src: 'https://example.com/fonts/bold.woff2', weight: '700', style: 'normal' },
    { family: 'Variable', src: 'https://cdn.example/italic.woff2', weight: '400 900', style: 'italic' },
  ]);
  const { prepareSnapshotFontReplacements } = require('../dist/lib/snapshot-fonts');
  const html = '<html><head><style>' + faces.map(fontFaceRuleCss).join('') + '</style></head><body>' +
    '<span style="font-family:Remote;font-weight:400">Bold</span>' +
    '<span style="font-family:Remote;font-weight:900">Black</span></body></html>';
  const catalog = ['Regular', 'Bold', 'Black'].map(style => ({ fontName: { family: 'Remote', style } }));
  expect(prepareSnapshotFontReplacements(html, catalog).replacements).toEqual([
    { families: 'Remote', weight: 400, style: 'normal', targetFamily: 'Remote', targetStyle: 'Bold' },
    { families: 'Remote', weight: 900, style: 'normal', targetFamily: 'Remote', targetStyle: 'Black' },
  ]);
});


test('prefers a loadable local or inline source over HTTP declarations', () => {
  expect(collectCarryOverFontFaces([sheet([
    fontFaceRule('Mixed', 'url(https://cdn.example/font.woff2), url(local.ttf)'),
    fontFaceRule('Inline', 'url(https://cdn.example/font.woff2), url(data:font/ttf;base64,AA)'),
  ])], PAGE, true).map(face => face.src)).toEqual([
    '/tmp/export/local.ttf', 'data:font/ttf;base64,AA',
  ]);
});
