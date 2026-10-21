'use strict';
const assert = require('node:assert/strict');
const { it } = require('node:test');
const { launchBrowser } = require('../../dist/lib/browser-engine');
const { takeSnapshot } = require('../../dist/lib/browser-snapshot');

it('captures inherited width and CSS-selected web font subsets', async () => {
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
        { from: 'WebFixture', path: '/normal.ttf', weight: '700', stretch: '100%' },
        { from: 'WebFixture', path: '/narrow.ttf', weight: '900', stretch: '75%' },
      ];
    });
    const snapshot = await page.evaluate(takeSnapshot);
    assert.match(snapshot.html, /font-stretch: 75%/);
    const bindings = await page.evaluate(() => window.__pagxFontBindings);
    assert.ok(bindings.some(b => b.faces.some(f => f.style === '800 75%')));
    const web = bindings.find(b => b.faces.some(f => f.path === '/normal.ttf'));
    assert.ok(web);
    assert.ok(web.faces.every(f => f.path !== '/narrow.ttf'));
    assert.ok(web.faces.some(f => f.style === '800 100%'));
  } finally { await browser.close(); }
});
