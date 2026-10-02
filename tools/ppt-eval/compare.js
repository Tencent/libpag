#!/usr/bin/env node
'use strict';

const fs = require('fs');
const { PNG } = require('pngjs');
const pixelmatchModule = require('pixelmatch');
const pixelmatch = typeof pixelmatchModule === 'function'
  ? pixelmatchModule
  : pixelmatchModule.default;

function loadPng(filePath) {
  return PNG.sync.read(fs.readFileSync(filePath));
}

function flattenOverWhite(image) {
  const data = image.data;
  for (let i = 0; i < data.length; i += 4) {
    const alpha = data[i + 3];
    if (alpha === 255) continue;
    const inverse = 255 - alpha;
    data[i] = Math.round((data[i] * alpha + 255 * inverse) / 255);
    data[i + 1] = Math.round((data[i + 1] * alpha + 255 * inverse) / 255);
    data[i + 2] = Math.round((data[i + 2] * alpha + 255 * inverse) / 255);
    data[i + 3] = 255;
  }
  return image;
}

function padToCommonSize(first, second) {
  const width = Math.max(first.width, second.width);
  const height = Math.max(first.height, second.height);
  const pad = (image) => {
    if (image.width === width && image.height === height) return image;
    const output = new PNG({ width, height });
    output.data.fill(255);
    for (let y = 0; y < image.height; y++) {
      const sourceStart = y * image.width * 4;
      const targetStart = y * width * 4;
      image.data.copy(output.data, targetStart, sourceStart, sourceStart + image.width * 4);
    }
    return output;
  };
  return [pad(first), pad(second), width, height];
}

function contentBounds(image, threshold = 2) {
  let left = image.width;
  let top = image.height;
  let right = -1;
  let bottom = -1;
  for (let y = 0; y < image.height; y++) {
    for (let x = 0; x < image.width; x++) {
      const offset = (y * image.width + x) * 4;
      const delta = Math.max(
        Math.abs(255 - image.data[offset]),
        Math.abs(255 - image.data[offset + 1]),
        Math.abs(255 - image.data[offset + 2]),
      );
      if (delta <= threshold) continue;
      left = Math.min(left, x);
      top = Math.min(top, y);
      right = Math.max(right, x);
      bottom = Math.max(bottom, y);
    }
  }
  if (right < left || bottom < top) {
    return { x: 0, y: 0, width: image.width, height: image.height };
  }
  const margin = 2;
  left = Math.max(0, left - margin);
  top = Math.max(0, top - margin);
  right = Math.min(image.width - 1, right + margin);
  bottom = Math.min(image.height - 1, bottom + margin);
  return { x: left, y: top, width: right - left + 1, height: bottom - top + 1 };
}

function imageMetrics(first, second, width, height, rect) {
  const x0 = rect ? rect.x : 0;
  const y0 = rect ? rect.y : 0;
  const x1 = rect ? rect.x + rect.width : width;
  const y1 = rect ? rect.y + rect.height : height;
  const pixelCount = Math.max(1, (x1 - x0) * (y1 - y0));
  const lumaFirst = new Float32Array(pixelCount);
  const lumaSecond = new Float32Array(pixelCount);
  let rgbDeltaTotal = 0;
  let sumFirst = 0;
  let sumSecond = 0;
  let index = 0;
  for (let y = y0; y < y1; y++) {
    for (let x = x0; x < x1; x++) {
      const offset = (y * width + x) * 4;
      const ar = first[offset];
      const ag = first[offset + 1];
      const ab = first[offset + 2];
      const br = second[offset];
      const bg = second[offset + 1];
      const bb = second[offset + 2];
      rgbDeltaTotal += (Math.abs(ar - br) + Math.abs(ag - bg) + Math.abs(ab - bb)) / 3;
      const firstLuma = 0.2126 * ar + 0.7152 * ag + 0.0722 * ab;
      const secondLuma = 0.2126 * br + 0.7152 * bg + 0.0722 * bb;
      lumaFirst[index] = firstLuma;
      lumaSecond[index] = secondLuma;
      sumFirst += firstLuma;
      sumSecond += secondLuma;
      index++;
    }
  }
  const meanFirst = sumFirst / pixelCount;
  const meanSecond = sumSecond / pixelCount;
  let varianceFirst = 0;
  let varianceSecond = 0;
  let covariance = 0;
  for (let i = 0; i < pixelCount; i++) {
    const firstDelta = lumaFirst[i] - meanFirst;
    const secondDelta = lumaSecond[i] - meanSecond;
    varianceFirst += firstDelta * firstDelta;
    varianceSecond += secondDelta * secondDelta;
    covariance += firstDelta * secondDelta;
  }
  varianceFirst /= pixelCount;
  varianceSecond /= pixelCount;
  covariance /= pixelCount;
  const c1 = (0.01 * 255) ** 2;
  const c2 = (0.03 * 255) ** 2;
  const numerator = (2 * meanFirst * meanSecond + c1) * (2 * covariance + c2);
  const denominator = (meanFirst ** 2 + meanSecond ** 2 + c1) *
    (varianceFirst + varianceSecond + c2);
  return {
    ssim: denominator === 0 ? 1 : numerator / denominator,
    meanRgbDelta: rgbDeltaTotal / pixelCount,
  };
}

function comparePng(referencePath, actualPath, diffPath) {
  const originalReference = loadPng(referencePath);
  const referenceWidth = originalReference.width;
  const referenceHeight = originalReference.height;
  const actualImage = loadPng(actualPath);
  const actualWidth = actualImage.width;
  const actualHeight = actualImage.height;
  const reference = flattenOverWhite(originalReference);
  const actual = flattenOverWhite(actualImage);
  const [paddedReference, paddedActual, width, height] = padToCommonSize(reference, actual);
  const diff = new PNG({ width, height });
  const differingPixels = pixelmatch(
    paddedReference.data,
    paddedActual.data,
    diff.data,
    width,
    height,
    { threshold: 0.1, includeAA: false },
  );
  if (diffPath) fs.writeFileSync(diffPath, PNG.sync.write(diff));
  const whole = imageMetrics(paddedReference.data, paddedActual.data, width, height);
  const roi = contentBounds(paddedReference);
  const roiMetrics = imageMetrics(paddedReference.data, paddedActual.data, width, height, roi);
  return {
    width,
    height,
    referenceWidth,
    referenceHeight,
    actualWidth,
    actualHeight,
    sizeMismatch: referenceWidth !== actualWidth || referenceHeight !== actualHeight,
    pixelDiffRatio: differingPixels / (width * height),
    meanRgbDelta: whole.meanRgbDelta,
    ssim: whole.ssim,
    roi,
    roiMeanRgbDelta: roiMetrics.meanRgbDelta,
    roiSsim: roiMetrics.ssim,
  };
}

module.exports = { comparePng };

if (require.main === module) {
  const [, , referencePath, actualPath, diffPath] = process.argv;
  if (!referencePath || !actualPath) {
    console.error('Usage: compare.js <reference.png> <actual.png> [diff.png]');
    process.exit(2);
  }
  console.log(JSON.stringify(comparePng(referencePath, actualPath, diffPath), null, 2));
}
