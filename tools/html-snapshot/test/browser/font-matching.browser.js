'use strict';
const assert = require('node:assert/strict');
const { it } = require('node:test');
const { launchBrowser } = require('../../dist/lib/browser-engine');
const { takeSnapshot } = require('../../dist/lib/browser-snapshot');

it('captures inherited width and CSS-selected web font aliases', async () => {
  const { browser } = await launchBrowser();
  try {
    const page = await browser.newPage();
    await page.setViewport({ width: 900, height: 300, deviceScaleFactor: 1 });
    await page.setContent(`<style>
      body{margin:0;width:900px;height:300px}
      .narrow{font-family:Helvetica;font-stretch:75%;font-weight:800}
      .narrow::before{content:'Pseudo';display:block}
    </style><div class="narrow"><span>Narrow</span></div>
    <div style="font-family:WebFixture,Helvetica;font-weight:800">Web text</div>`);
    await page.evaluate(() => {
      window.__pagxFontAliases = [
        { from: 'WebFixture', to: 'WebFixtureNormal', weight: '700', stretch: '100%' },
        { from: 'WebFixture', to: 'WebFixtureNarrow', weight: '900', stretch: '75%' },
      ];
    });
    const snapshot = await page.evaluate(takeSnapshot);
    assert.match(snapshot.html, /font-stretch: 75%/);
    assert.match(snapshot.html, /font-weight: 800/);
    assert.match(snapshot.html, /font-family: [^;]*WebFixtureNormal[^;]*Helvetica/);
    assert.doesNotMatch(snapshot.html, /WebFixtureNarrow/);
  } finally { await browser.close(); }
});

it('preserves HTTP face descriptors and CSS catalog names in both snapshot modes', async () => {
  const { prepareSnapshotFontReplacements } = require('../../dist/lib/snapshot-fonts');
  const { browser } = await launchBrowser();
  try {
    for (const mode of ['normal', 'current-state']) {
      const page = await browser.newPage();
      try {
        await page.setViewport({ width: 900, height: 300, deviceScaleFactor: 1 });
        await page.setContent(`<style>
          @font-face{font-family:Remote;src:url(https://example.invalid/bold.woff2);font-weight:700}
          @font-face{font-family:Playfair;src:url(data:font/woff2;base64,AA);font-weight:400 900;font-style:italic}
          body{margin:0;width:900px;height:300px}
        </style><div style="font-family:Remote;font-weight:400">Bold</div>
        <div style="font-family:Remote;font-weight:900">Black</div>
        <div style="font-family:Playfair;font-weight:500;font-style:italic">Ag</div>`,
        { waitUntil: 'domcontentloaded' });
        await page.evaluate(() => {
          window.__pagxFontAliases = [
            { from: 'Playfair', to: 'Playfair Display', weight: '400 900', style: 'italic' },
          ];
        });
        const snapshot = await page.evaluate(takeSnapshot, mode === 'current-state' ? { mode } : {});
        const catalog = ['Regular', 'Bold', 'Black'].map(style => ({ fontName: { family: 'Remote', style } }));
        catalog.push({ fontName: { family: 'Playfair', style: 'Medium Italic' } });
        const { replacements } = prepareSnapshotFontReplacements(snapshot.html, catalog);
        assert.deepEqual(replacements.map(r => [r.weight, r.targetFamily, r.targetStyle]), [
          [400, 'Remote', 'Bold'], [900, 'Remote', 'Black'], [500, 'Playfair', 'Medium Italic'],
        ], mode);
        assert.match(snapshot.html, /font-family: Playfair, 'Playfair Display'/);
        assert.match(snapshot.html, /font-weight:700;font-style:normal/);
      } finally { await page.close(); }
    }
  } finally { await browser.close(); }
});
