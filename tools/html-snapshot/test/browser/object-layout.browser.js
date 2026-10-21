'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { spawnSync } = require('node:child_process');
const { describe, it, before, after } = require('node:test');
const { PNG } = require('pngjs');
const { launchBrowser } = require('../../dist/lib/browser-engine');
const { takeSnapshot, inlineExternalImages } = require('../../dist/lib/browser-snapshot');

const binary = process.env.PAGX_HTML_TEST_BIN ||
  path.resolve(__dirname, '../../../../cmake-build-debug/pagx');
const png = new PNG({ width: 120, height: 80 });
for (let y = 0; y < 80; y++) for (let x = 0; x < 120; x++) {
  const at = (y * 120 + x) * 4;
  png.data.set(x < 60 ? (y < 40 ? [255, 0, 0, 255] : [255, 255, 0, 255]) : (y < 40 ? [0, 0, 255, 128] : [0, 255, 255, 255]), at);
}
const image = `data:image/png;base64,${PNG.sync.write(png).toString('base64')}`;

describe('CSS replaced image layout agrees with Chromium', { skip: !fs.existsSync(binary) }, () => {
  let browser, page;
  before(async () => {
    ({ browser } = await launchBrowser());
    page = await browser.newPage();
  });
  after(async () => { if (browser) await browser.close(); });

  async function compare(name, style, {
    scale = 1, tag = 'img', contents = '', mean = 2, flatTolerance = 4, direct = true, src = image, attrs = '', container = '',
  } = {}) {
    await page.setViewport({ width: 240, height: 200, deviceScaleFactor: scale });
    const html = `<html><body style="margin:0;width:240px;height:200px;background:#00ff00">
      ${container}<${tag} ${tag === 'img' ? `src="${src}" ${attrs}` : ''}
      style="box-sizing:border-box;position:absolute;left:10px;top:10px;width:220px;height:180px;${style.replaceAll('"', '&quot;')}"${tag === 'img' ? '/' : ''}>
      ${contents}${tag === 'img' ? '' : `</${tag}>`}${container ? (container.startsWith('<picture') ? '</picture>' : '</div>') : ''}</body></html>`;
    await page.setContent(html);
    await page.evaluate(async () => { await Promise.all([...document.images].map(i => i.decode())); });
    const reference = PNG.sync.read(Buffer.from(await page.screenshot()));
    await page.evaluate(inlineExternalImages, {});
    const snapshot = await page.evaluate(takeSnapshot);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), `pagx-object-${name}-`));
    fs.writeFileSync(path.join(dir, 'reference.png'), PNG.sync.write(reference));
    await page.setContent(snapshot.html);
    await page.addStyleTag({ content: 'html{padding:0;display:block;min-height:0}' });
    await page.evaluate(async () => { await Promise.all([...document.images].map(i => i.decode().catch(() => {}))); });
    fs.writeFileSync(path.join(dir, 'preview.png'), Buffer.from(await page.screenshot()));
    const sources = direct ? [['direct', html], ['snapshot', snapshot.html], ['preview', null]] : [['snapshot', snapshot.html], ['preview', null]];
    for (const [kind, source] of sources) {
      const input = path.join(dir, `${kind}.html`), output = path.join(dir, `${kind}.pagx`);
      if (source !== null) {
        fs.writeFileSync(input, source);
        for (const args of [
          ['import', '--input', input, '--output', output, '--format', 'html'],
          ['render', output, '-o', path.join(dir, `${kind}.png`), '--scale', String(scale)],
        ]) {
          const result = spawnSync(binary, args, {
            env: { ...process.env, PAGX_HTML_SNAPSHOT: '0' }, encoding: 'utf8',
          });
          assert.equal(result.status, 0, `${name}/${kind}: ${result.stderr}; ${dir}`);
        }
      }
      const actual = PNG.sync.read(fs.readFileSync(path.join(dir, `${kind}.png`)));
      assert.equal(actual.width, reference.width);
      assert.equal(actual.height, reference.height);
      let sum = 0, flatError = 0;
      for (let y = 0; y < actual.height; y++) for (let x = 0; x < actual.width; x++) {
        const at = (y * actual.width + x) * 4;
        for (let c = 0; c < 4; c++) sum += Math.abs(actual.data[at + c] - reference.data[at + c]);
        if (x < 4 || y < 4 || x >= actual.width - 4 || y >= actual.height - 4) continue;
        let flat = true;
        for (let dy = -4; dy <= 4 && flat; dy++) for (let dx = -4; dx <= 4 && flat; dx++) {
          const near = ((y + dy) * actual.width + x + dx) * 4;
          for (let c = 0; c < 4; c++) if (reference.data[at + c] !== reference.data[near + c]) flat = false;
        }
        if (flat) for (let c = 0; c < 4; c++) {
          flatError = Math.max(flatError, Math.abs(actual.data[at + c] - reference.data[at + c]));
        }
      }
      assert.ok(flatError <= flatTolerance, `${name}/${kind}: flat error ${flatError}; ${dir}`);
      assert.ok(sum / actual.data.length < mean,
        `${name}/${kind}: mean ${sum / actual.data.length}; ${dir}`);
    }
    fs.rmSync(dir, { recursive: true });
  }

  for (const fit of ['fill', 'contain', 'cover', 'none', 'scale-down']) {
    for (const position of ['left top', 'right bottom', '20% 80%', 'calc(100% - 13px) calc(50% + 7px)']) {
      it(`${fit} at ${position}`, async () => {
        await compare(`${fit}-${position.replace(/[^a-z]/g, '')}`,
          `object-fit:${fit};object-position:${position};background:#222;border-radius:17px`);
      });
    }
  }
  it('clips a natural-size image larger than the content box', async () => {
    await compare('large-none', 'width:70px;height:50px;object-fit:none;object-position:right 7px bottom 9px;background:white');
  });
  it('does not enlarge a small scale-down image', async () => {
    await compare('small-scale-down', 'object-fit:scale-down;background:#222');
  });
  it('shrinks a large scale-down image', async () => {
    await compare('large-scale-down', 'width:70px;height:50px;object-fit:scale-down;object-position:100% 0;background:white');
  });
  it('resolves vertical cover overflow', async () => {
    await compare('vertical-cover', 'width:200px;height:60px;object-fit:cover;object-position:40% 80%');
  });
  it('keeps content separate from padding and border', async () => {
    await compare('content-box', 'object-fit:cover;object-position:20% 80%;padding:12px 17px 9px 6px;border:7px solid #888;border-radius:40px 30px / 50px 25px;background:#222',
      // Nested contour masks have a small antialiasing fringe; also constrain aggregate error.
      { flatTolerance: 16, mean: .5 });
    await compare('content-box-square', 'object-fit:cover;object-position:20% 80%;padding:12px 17px 9px 6px;border:7px solid #888;background:#222');
  });
  it('supports positions outside the usual percentage range', async () => {
    await compare('outside', 'object-fit:none;object-position:-20% 130%;background:#222');
  });
  it('uses final percentage and flex sizes', async () => {
    await compare('percent', 'width:50%;height:60%;object-fit:cover;object-position:right bottom',
      { container: '<div style="position:relative;width:200px;height:160px">' });
    await compare('flex', 'position:static;flex:1;width:60px;height:120px;object-fit:cover;object-position:right bottom',
      { container: '<div style="display:flex;width:200px;height:120px">' });
  });
  // Chromium currently clamps scale-down against encoded pixels even though naturalWidth is
  // density-corrected (none uses naturalWidth). The standard scale-down rule is covered by
  // HTMLObjectLayoutTest; use none for the browser density oracle.
  it('retains selected-resource density', async () => {
    await compare('density-none', 'object-fit:none;object-position:right bottom;background:#222',
      { attrs: `srcset="${image} 2x"`, direct: false });
  });
  it('captures a picture-selected source when img has no src', async () => {
    await page.setRequestInterception(true);
    const respond = request => request.respond({
      status: 200, contentType: 'image/png', body: PNG.sync.write(png),
      headers: { 'access-control-allow-origin': '*' },
    });
    page.on('request', respond);
    try {
      await compare('picture', 'object-fit:none;object-position:right bottom;background:#222', {
        src: '', direct: false,
        container: '<picture><source srcset="https://object-layout.test/selected.png 2x">',
      });
    } finally {
      page.off('request', respond);
      await page.setRequestInterception(false);
    }
  });
  for (const scale of [1.5, 2]) {
    it(`keeps fractional content geometry at DPR ${scale}`, async () => {
      await compare(`fractional-${scale}`, 'width:137.5px;height:81.25px;object-fit:cover;object-position:20% 80%;border-radius:7px', { scale });
    });
  }
  it('applies effects and transforms once around the fitted contents', async () => {
    await compare('effects', 'width:160px;height:100px;object-fit:none;object-position:right bottom;' +
      'transform:translate(7px,9px);opacity:.6;border-radius:15px;box-shadow:0 3px 5px #0008;background:#222');
  });
  it('uses oriented natural dimensions for an EXIF-rotated image', async () => {
    const oriented = 'data:image/jpeg;base64,/9j/4AAQSkZJRgABAQAAAQABAAD/4QAiRXhpZgAATU0AKgAAAAgAAQESAAMAAAABAAYAAAAAAAD/2wBDAAEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQH/2wBDAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQH/wAARCAAUACgDAREAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwD8X6/ynP8Av4CgD5D/AGqv+ZD/AO5o/wDddr/qo/0Zf/nNj/vW7/4PZ/zX/wCkM/8AOIf/AHn7/wCAofIdf9VB/wA14UAfrxX/AJV5/wClAFAHyH+1V/zIf/c0f+67X/VR/oy//ObH/et3/wAHs/5r/wDSGf8AnEP/ALz9/wDAUPkOv+qg/wCa8KAP14r/AMq8/wDSgCgD5D/aq/5kP/uaP/ddr/qo/wBGX/5zY/71u/8Ag9n/ADX/AOkM/wDOIf8A3n7/AOAofIdf9VB/zXhQB//Z';
    await compare('exif', 'object-fit:none;object-position:right bottom;background:#222', { src: oriented });
  });
  for (const [kind, root] of [
    ['sized', 'width="120" height="80" viewBox="0 0 120 80"'],
    ['ratio', 'viewBox="0 0 120 80"'],
    ['viewport', ''],
    ['stretch', 'width="120" height="80" viewBox="0 0 120 80" preserveAspectRatio="none"'],
  ]) {
    for (const fit of ['fill', 'contain', 'cover', 'none', 'scale-down']) {
      it(`SVG ${kind} ${fit}`, async () => {
        const svg = `<svg xmlns="http://www.w3.org/2000/svg" ${root}><rect width="50%" height="100%" fill="red"/><circle cx="75%" cy="50%" r="15" fill="blue"/></svg>`;
        await compare(`svg-${kind}-${fit}`, `object-fit:${fit};object-position:right bottom;background:#222;border-radius:9px`,
          { src: `data:image/svg+xml;base64,${Buffer.from(svg).toString('base64')}`,
            // Chromium synthesizes a 300x150 natural viewport for dimensionless SVG. The
            // snapshot captures it; direct import uses the CSS default sizing algorithm.
            direct: kind !== 'viewport' && !(kind === 'ratio' && fit === 'none') });
      });
    }
  }
});
