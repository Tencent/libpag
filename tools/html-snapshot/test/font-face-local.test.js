'use strict';

// Regression tests for the snapshot's local web-font carry-over. The subset
// rebuilds the page with inline styles only, so without these rules the
// importer never learns which font file a `font-family` name stands for, and
// a design export whose text lives in `assets/fonts/*.woff2` renders blank.
// `collectLocalFontFaces` reads the page's `@font-face` rules and
// `cssQuotedString` writes them back so both the browser and the importer's
// XML + CSS readers decode the same strings.

const { collectLocalFontFaces, cssQuotedString } = require('../dist/lib/browser-snapshot');

function fontFaceRule(family, src) {
  return {
    type: 5,
    style: {
      getPropertyValue: (name) => {
        if (name === 'font-family') return family;
        if (name === 'src') return src;
        return '';
      },
    },
  };
}

function sheet(rules, href) {
  return { href: href || null, cssRules: rules };
}

const PAGE = 'file:///tmp/export/index.html';

describe('collectLocalFontFaces', () => {
  test('keeps the first local url() source resolved against the page', () => {
    const faces = collectLocalFontFaces([
      sheet([
        fontFaceRule(
          '"pagx-font-f1"',
          'local("Missing"), url("https://cdn.example/f1.woff2") format("woff2"), ' +
            'url("assets/fonts/font_f1.woff2") format("woff2"), url(font_f1.ttf)',
        ),
      ]),
    ], PAGE);
    expect(faces).toEqual([
      { family: 'pagx-font-f1', path: '/tmp/export/assets/fonts/font_f1.woff2' },
    ]);
  });

  test('drops rules that only have remote or data: sources', () => {
    const faces = collectLocalFontFaces([
      sheet([
        fontFaceRule('Remote', 'url(https://cdn.example/remote.woff2)'),
        fontFaceRule('Inline', 'url("data:font/woff2;base64,AAAA")'),
        fontFaceRule('Named', 'local("Arial")'),
      ]),
    ], PAGE);
    expect(faces).toEqual([]);
  });

  test('resolves against the stylesheet URL and percent-decodes the path', () => {
    const faces = collectLocalFontFaces([
      sheet([fontFaceRule("'My Font'", "url('../fonts/My%20Font.woff2')")],
        'file:///tmp/export/css/site.css'),
    ], PAGE);
    expect(faces).toEqual([{ family: 'My Font', path: '/tmp/export/fonts/My Font.woff2' }]);
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
    const faces = collectLocalFontFaces([
      inaccessible,
      sheet([
        { styleSheet: imported },
        { type: 4, cssRules: [fontFaceRule('Media', 'url(media.woff2)')] },
      ]),
    ], PAGE);
    expect(faces).toEqual([
      { family: 'Imported', path: '/tmp/export/css/imported.woff2' },
      { family: 'Media', path: '/tmp/export/media.woff2' },
    ]);
  });

  test('reports each (family, file) pair once', () => {
    const rule = fontFaceRule('Brand', 'url(brand.woff2)');
    const faces = collectLocalFontFaces([sheet([rule, rule])], PAGE);
    expect(faces).toEqual([{ family: 'Brand', path: '/tmp/export/brand.woff2' }]);
  });

  test('tolerates a missing stylesheet list', () => {
    expect(collectLocalFontFaces(null, PAGE)).toEqual([]);
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
