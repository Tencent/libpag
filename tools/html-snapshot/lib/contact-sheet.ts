// Copyright (C) 2026 Tencent. All rights reserved.
// Licensed under the Apache License, Version 2.0.

import * as fs from 'fs/promises';
import * as path from 'path';
import { PNG } from 'pngjs';
import { defaultPagxBin, runPagxRender } from './pipeline';

export interface ContactSheetItem {
  name: string;
  png: string;
}

export interface ContactSheetImage extends ContactSheetItem {
  width: number;
  height: number;
}

export interface ContactSheetLayoutOptions {
  columns?: number;
  thumbnailWidth?: number;
  thumbnailHeight?: number;
  maxEdge?: number;
  maxPixels?: number;
  maxSourcePixels?: number;
}

export interface ContactSheetOptions extends ContactSheetLayoutOptions {
  outputDir: string;
  pagxBin?: string;
  log?: (line: string) => void;
}

export interface ContactSheetPlacement extends ContactSheetImage {
  index: number;
  left: number;
  top: number;
  thumbnailWidth: number;
  thumbnailHeight: number;
}

export interface ContactSheetLayout {
  width: number;
  height: number;
  columns: number;
  cellWidth: number;
  cellHeight: number;
  items: ContactSheetPlacement[];
}

export interface ContactSheetPage {
  /** Absolute output path; callers choose how to serialize it in their manifest. */
  file: string;
  width: number;
  height: number;
  items: string[];
}

export interface ContactSheetResult {
  pages: ContactSheetPage[];
}

export const CONTACT_SHEET_DEFAULTS = Object.freeze({
  thumbnailWidth: 320,
  thumbnailHeight: 480,
  maxEdge: 4096,
  maxPixels: 16_000_000,
  maxSourcePixels: 32_000_000,
});
const MAX_PNG_BYTES = 128 * 1024 * 1024;
const MARGIN = 20, GAP = 20, PADDING = 12, LABEL_HEIGHT = 64;
const PNG_SIGNATURE = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);

export function validateContactSheetColumns(columns: number | undefined): void {
  if (columns !== undefined && (!Number.isSafeInteger(columns) || columns < 1 || columns > 32)) {
    throw new Error('preview columns must be an integer from 1 to 32');
  }
}

export interface PreviewOptions {
  preview?: boolean;
  previewColumns?: number;
  doRender?: boolean;
  doResolve?: boolean;
}

export function validatePreviewOptions(options: PreviewOptions): void {
  if (options.previewColumns !== undefined && !options.preview) throw new Error('--preview-columns requires --preview');
  validateContactSheetColumns(options.previewColumns);
  if (options.preview && (options.doRender === false || options.doResolve === false)) {
    throw new Error('--preview requires rendering; it cannot be combined with --no-render or --no-resolve');
  }
}

function layoutOptions(options: ContactSheetLayoutOptions) {
  validateContactSheetColumns(options.columns);
  const limits: Record<keyof typeof CONTACT_SHEET_DEFAULTS, number> = { ...CONTACT_SHEET_DEFAULTS };
  for (const key of Object.keys(limits) as (keyof typeof limits)[]) {
    const value = options[key] ?? limits[key];
    if (!Number.isSafeInteger(value) || value < 1 || value > CONTACT_SHEET_DEFAULTS[key]) {
      throw new Error(`${key} must be an integer from 1 to ${CONTACT_SHEET_DEFAULTS[key]}`);
    }
    limits[key] = value;
  }
  return { ...limits, columns: options.columns };
}

/** Pure layout: preserve input order, contain whole images, and paginate within both budgets. */
export function layoutContactSheet(
  images: readonly ContactSheetImage[], options: ContactSheetLayoutOptions = {},
): ContactSheetLayout[] {
  const limits = layoutOptions(options);
  if (!images.length) throw new Error('contact sheet requires at least one PNG');
  const thumbnails = images.map((image, index) => {
    const { width, height } = image;
    if (!Number.isSafeInteger(width) || !Number.isSafeInteger(height) || width < 1 || height < 1 ||
        width * height > limits.maxSourcePixels) {
      throw new Error(`PNG '${image.name}' has invalid dimensions or exceeds the source pixel budget (${limits.maxSourcePixels})`);
    }
    const scale = Math.min(1, limits.thumbnailWidth / width, limits.thumbnailHeight / height);
    return { ...image, index, left: 0, top: 0, thumbnailWidth: width * scale, thumbnailHeight: height * scale };
  });
  function fit(items: ContactSheetPlacement[]): ContactSheetLayout | undefined {
    if (items.reduce((sum, item) => sum + item.width * item.height, 0) > limits.maxSourcePixels) return undefined;
    const cellWidth = Math.max(160, Math.ceil(Math.max(...items.map(item => item.thumbnailWidth)))) + PADDING * 2;
    const cellHeight = LABEL_HEIGHT + Math.ceil(Math.max(...items.map(item => item.thumbnailHeight))) + PADDING;
    const maxColumns = Math.min(limits.columns ?? 6, items.length);
    let best: ContactSheetLayout | undefined;
    let bestScore = Infinity;
    for (let columns = limits.columns ? maxColumns : 1; columns <= maxColumns; columns++) {
      const rows = Math.ceil(items.length / columns);
      const width = MARGIN * 2 + columns * cellWidth + (columns - 1) * GAP;
      const height = MARGIN * 2 + rows * cellHeight + (rows - 1) * GAP;
      if (width > limits.maxEdge || height > limits.maxEdge || width * height > limits.maxPixels) continue;
      const score = Math.abs(Math.log((width / height) / (4 / 3)));
      if (score >= bestScore) continue;
      bestScore = score;
      best = { width, height, columns, cellWidth, cellHeight, items: items.map((item, i) => ({
        ...item,
        left: MARGIN + (i % columns) * (cellWidth + GAP),
        top: MARGIN + Math.floor(i / columns) * (cellHeight + GAP),
      })) };
    }
    return best;
  }
  const pages: ContactSheetLayout[] = [];
  let current: ContactSheetLayout | undefined;
  for (const thumbnail of thumbnails) {
    const next = fit([...(current?.items ?? []), thumbnail]);
    if (next) { current = next; continue; }
    if (current) pages.push(current);
    current = fit([thumbnail]);
    if (!current) throw new Error(`PNG '${thumbnail.name}' cannot fit within the contact sheet output budget`);
  }
  if (current) pages.push(current);
  return pages;
}

/** Check bounds before allocating/decoding. Copy exactly the validated bytes to avoid rereading a changing input. */
async function validatePng(file: string, maxPixels: number, copyTo?: string): Promise<{ width: number; height: number }> {
  const handle = await fs.open(file, 'r');
  try {
    const stat = await handle.stat();
    if (!stat.isFile() || stat.size < 33 || stat.size > MAX_PNG_BYTES) {
      throw new Error(`PNG file size must be between 33 and ${MAX_PNG_BYTES} bytes`);
    }
    const header = Buffer.alloc(24);
    await handle.read(header, 0, header.length, 0);
    if (!header.subarray(0, 8).equals(PNG_SIGNATURE) || header.readUInt32BE(8) !== 13 || header.toString('ascii', 12, 16) !== 'IHDR') {
      throw new Error('invalid PNG signature or IHDR');
    }
    const width = header.readUInt32BE(16), height = header.readUInt32BE(20);
    if (!width || !height || width * height > maxPixels) {
      throw new Error(`PNG dimensions ${width}x${height} exceed the pixel budget (${maxPixels}) or are invalid`);
    }
    const bytes = Buffer.alloc(stat.size);
    let offset = 0;
    while (offset < bytes.length) {
      const { bytesRead } = await handle.read(bytes, offset, bytes.length - offset, offset);
      if (!bytesRead) throw new Error('truncated PNG');
      offset += bytesRead;
    }
    // Native image loading may silently ignore corrupt inputs, so signature-only validation is insufficient.
    const decoded = PNG.sync.read(bytes, { checkCRC: true });
    if (decoded.width !== width || decoded.height !== height) throw new Error('PNG dimensions changed while reading');
    if (copyTo) await fs.writeFile(copyTo, bytes);
    return { width, height };
  } finally { await handle.close(); }
}

function xml(value: string): string {
  return value.replace(/[\u0000-\u0008\u000b\u000c\u000e-\u001f\ufffe\uffff]/g, '\ufffd')
    .replace(/[\r\n\t]/g, ' ').replace(/&/g, '&amp;').replace(/</g, '&lt;')
    .replace(/>/g, '&gt;').replace(/"/g, '&quot;').replace(/'/g, '&apos;');
}

function sheetDocument(page: ContactSheetLayout): string {
  const layers = [`<Layer><Rectangle width="${page.width}" height="${page.height}"/><Fill color="#ECEEF1"/></Layer>`];
  for (const item of page.items) {
    const x = item.left + (page.cellWidth - item.thumbnailWidth) / 2, y = item.top + LABEL_HEIGHT;
    const rect = `<Rectangle left="${x}" top="${y}" width="${item.thumbnailWidth}" height="${item.thumbnailHeight}"/>`;
    layers.push(`<Layer><Rectangle left="${item.left}" top="${item.top}" width="${page.cellWidth}" height="${page.cellHeight}"/><Fill color="#FFFFFF"/></Layer>`,
      `<Layer><TextBox left="${item.left + PADDING}" top="${item.top + 10}" width="${page.cellWidth - PADDING * 2}" height="36" lineHeight="18" overflow="hidden"><Text text="${xml(`${item.index + 1}. ${item.name}`)}" fontSize="14"/><Fill color="#24282F"/></TextBox></Layer>`,
      `<Layer><TextBox left="${item.left + PADDING}" top="${item.top + 46}" width="${page.cellWidth - PADDING * 2}" height="16" lineHeight="16" overflow="hidden"><Text text="${item.width} × ${item.height} px" fontSize="12"/><Fill color="#69717D"/></TextBox></Layer>`,
      `<Layer>${rect}<Fill><ImagePattern image="checker.png" scaleMode="none" tileModeX="repeat" tileModeY="repeat"/></Fill></Layer>`,
      `<Layer>${rect}<Fill><ImagePattern image="input-${item.index}.png" scaleMode="letterBox"/></Fill></Layer>`);
  }
  return `<pagx width="${page.width}" height="${page.height}">\n${layers.join('\n')}\n</pagx>\n`;
}

function checkerPng(): Buffer {
  const png = new PNG({ width: 16, height: 16 });
  for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) {
    const value = (x < 8) === (y < 8) ? 240 : 214;
    png.data.set([value, value, value, 255], (y * 16 + x) * 4);
  }
  return PNG.sync.write(png);
}

/** Render only the supplied PNGs, without a browser, directory scan, or scenario replay. */
export async function generateContactSheet(
  items: readonly ContactSheetItem[], options: ContactSheetOptions,
): Promise<ContactSheetResult> {
  const limits = layoutOptions(options);
  if (!items.length) throw new Error('contact sheet requires at least one PNG');
  const outputDir = path.resolve(options.outputDir);
  await fs.mkdir(outputDir, { recursive: true });
  // Stage on the same filesystem as final files so each rename is atomic.
  const staging = await fs.mkdtemp(path.join(outputDir, '.tmp-'));
  try {
    const images: ContactSheetImage[] = [];
    for (const [index, item] of items.entries()) {
      try {
        const size = await validatePng(item.png, limits.maxSourcePixels, path.join(staging, `input-${index}.png`));
        images.push({ ...item, ...size });
      } catch (error) {
        throw new Error(`invalid preview input '${item.name}' (${item.png}): ${error instanceof Error ? error.message : String(error)}`);
      }
    }
    const layouts = layoutContactSheet(images, options);
    const inputPaths = new Set(await Promise.all(items.map(item => fs.realpath(item.png))));
    await fs.writeFile(path.join(staging, 'checker.png'), checkerPng());
    const pages: ContactSheetPage[] = [];
    for (const [index, layout] of layouts.entries()) {
      const fileName = `sheet-${String(index + 1).padStart(3, '0')}.png`;
      const file = path.join(outputDir, fileName);
      const destination = await fs.realpath(file).catch((error: NodeJS.ErrnoException) => {
        if (error.code === 'ENOENT') return file;
        throw error;
      });
      if (inputPaths.has(destination)) throw new Error(`preview output would overwrite an input PNG: ${file}`);
      const pagxFile = path.join(staging, 'sheet.pagx'), output = path.join(staging, fileName);
      await fs.writeFile(pagxFile, sheetDocument(layout));
      options.log?.(`[preview] ${index + 1}/${layouts.length}: ${layout.items.length} images, ${layout.width}x${layout.height}`);
      const result = await runPagxRender({ pagxBin: options.pagxBin || defaultPagxBin(), pagxFile, output, scale: 1 });
      if (result.code !== 0) throw new Error(`preview render failed (exit ${result.code}): ${result.stderr || result.stdout}`);
      const rendered = await validatePng(output, limits.maxPixels);
      if (rendered.width !== layout.width || rendered.height !== layout.height) {
        throw new Error(`preview render returned unexpected dimensions: ${rendered.width}x${rendered.height}`);
      }
      pages.push({ file, width: layout.width, height: layout.height,
        items: layout.items.map(item => item.name) });
    }
    // No previously published sheet is touched until every page has rendered successfully.
    // The caller's manifest is the authoritative list; older, unlisted sheets are not deleted.
    for (const page of pages) await fs.rename(path.join(staging, path.basename(page.file)), page.file);
    return { pages };
  } finally { await fs.rm(staging, { recursive: true, force: true }); }
}
