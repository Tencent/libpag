// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
import { promises as fs } from 'node:fs';
import { PNG } from 'pngjs';
const MAX_PNG_BYTES = 128 * 1024 * 1024;
const PNG_SIGNATURE = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);

/** Check bounds before allocating/decoding. Copy exactly the validated bytes to avoid rereading a changing input. */
export async function validatePng(file: string, maxPixels: number, copyTo?: string): Promise<{ width: number; height: number }> {
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
