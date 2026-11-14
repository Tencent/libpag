#!/usr/bin/env node
'use strict';

const fs = require('node:fs');
const path = require('node:path');
const { DOMParser } = require('@xmldom/xmldom');

// Count only the scene tree. Resource definitions and timeline targets are not scene children.
// Also count Group/TextBox nesting so changing a Layer to a Group cannot hide extra structure.
function measureHierarchy(xml) {
  const document = new DOMParser({ onError: level => {
    if (level === 'error' || level === 'fatalError') throw new Error('Invalid PAGX XML');
  } }).parseFromString(xml, 'application/xml');
  if (document.documentElement?.tagName !== 'pagx') throw new Error('Expected a PAGX document');
  const metrics = { layers: 0, layerDepth: 0, structureDepth: 0, groups: 0, masks: 0,
    internalPaintMarkers: 0, depthHistogram: {} };
  const maskIds = new Set(Array.from(document.getElementsByTagName('Layer'))
    .map(layer => layer.getAttribute('mask')).filter(Boolean).map(id => id.replace(/^@/, '')));
  function visit(node, layerDepth, structureDepth) {
    if (node.nodeType !== 1) return;
    if (node.tagName === 'Layer') {
      layerDepth++;
      structureDepth++;
      metrics.layers++;
      metrics.layerDepth = Math.max(metrics.layerDepth, layerDepth);
      metrics.depthHistogram[layerDepth] = (metrics.depthHistogram[layerDepth] || 0) + 1;
      if (maskIds.has(node.getAttribute('id'))) metrics.masks++;
      if (node.hasAttribute('data-pagx-paint-order')) metrics.internalPaintMarkers++;
    } else if (node.tagName === 'Group' || node.tagName === 'TextBox') {
      metrics.groups++;
      structureDepth++;
    } else return;
    metrics.structureDepth = Math.max(metrics.structureDepth, structureDepth);
    for (const child of Array.from(node.childNodes)) visit(child, layerDepth, structureDepth);
  }
  for (const node of Array.from(document.documentElement.childNodes)) visit(node, 0, 0);
  return metrics;
}

function filesAt(filename) {
  if (!fs.statSync(filename).isDirectory()) return filename.endsWith('.pagx') ? [filename] : [];
  return fs.readdirSync(filename).sort().flatMap(name => filesAt(path.join(filename, name)));
}

if (require.main === module) {
  if (process.argv.length < 3) throw new Error('Usage: node eval/hierarchy.js <file.pagx|directory> ...');
  const rows = process.argv.slice(2).flatMap(filesAt).map(file => ({
    file, ...measureHierarchy(fs.readFileSync(file, 'utf8')),
  }));
  process.stdout.write(JSON.stringify(rows, null, 2) + '\n');
}

module.exports = { measureHierarchy };
