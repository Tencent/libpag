/**
 * Puppeteer screenshot script for PAGX-to-HTML visual comparison.
 *
 * Usage: node screenshot.js <html_file> <output_png> <width> <height>
 *
 * The HTML file is expected to contain a PAGX HTML fragment (a <div class="pagx-root"> element).
 * This script wraps it in a minimal HTML document and takes a screenshot at the specified size.
 *
 * Prerequisites: npx puppeteer browsers install chrome
 */

const puppeteer = require('puppeteer');
const fs = require('fs');
const path = require('path');

const args = process.argv.slice(2);
if (args.length < 4) {
  console.error('Usage: node screenshot.js <html_file> <output_png> <width> <height>');
  process.exit(1);
}

const [htmlFile, outputPng, widthStr, heightStr] = args;
const width = parseInt(widthStr, 10);
const height = parseInt(heightStr, 10);

(async () => {
  const fragment = fs.readFileSync(path.resolve(htmlFile), 'utf-8');
  const fullHtml = `<!DOCTYPE html>
<html><head><meta charset="utf-8"></head>
<body style="margin:0;padding:0;background:transparent">
${fragment}
</body></html>`;

  const browser = await puppeteer.launch({
    headless: true,
    args: ['--no-sandbox', '--disable-setuid-sandbox', '--hide-scrollbars'],
  });
  const page = await browser.newPage();
  await page.setViewport({ width, height, deviceScaleFactor: 1 });
  await page.setContent(fullHtml, { waitUntil: 'networkidle0' });
  await page.screenshot({
    path: path.resolve(outputPng),
    type: 'png',
    omitBackground: true,
    clip: { x: 0, y: 0, width, height },
  });
  await browser.close();
})();
