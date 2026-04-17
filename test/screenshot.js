/**
 * Puppeteer screenshot script for PAGX-to-HTML visual comparison.
 *
 * Usage: node screenshot.js <html_file> <output_png> <width> <height>
 *
 * The HTML file is expected to be a complete HTML document (with <!DOCTYPE html>).
 * This script loads it directly via file:// URL and takes a screenshot at the specified size.
 *
 * Prerequisites: npx puppeteer browsers install chrome
 */

const puppeteer = require('puppeteer');
const path = require('path');

const args = process.argv.slice(2);
if (args.length < 4) {
  console.error('Usage: node screenshot.js <html_file> <output_png> <width> <height>');
  process.exit(1);
}

const [htmlFile, outputPng, widthStr, heightStr] = args;
const width = parseInt(widthStr, 10);
const height = parseInt(heightStr, 10);

const scale = args.length >= 5 ? parseInt(args[4], 10) : 1;

(async () => {
  const browser = await puppeteer.launch({
    headless: 'shell',
    protocolTimeout: 120000,
    args: ['--no-sandbox', '--disable-setuid-sandbox', '--hide-scrollbars'],
  });
  const page = await browser.newPage();
  await page.setViewport({ width, height, deviceScaleFactor: scale });
  const fileUrl = 'file://' + path.resolve(htmlFile);
  await page.goto(fileUrl, { waitUntil: 'networkidle0', timeout: 60000 });
  await page.screenshot({
    path: path.resolve(outputPng),
    type: 'png',
    omitBackground: true,
    clip: { x: 0, y: 0, width, height },
    timeout: 60000,
  });
  await browser.close();
})();
