// Versioned hand-off between browser capture and both native shaping commands.
import * as fs from 'fs';
import * as path from 'path';

export interface FontFaceSource {
  path?: string;
  systemFamily?: string;
  faceIndex?: number;
  style?: string;
  weight?: string;
  stretch?: string;
  variations?: string;
  unicodeRange?: string;
}
export interface FontBinding { family: string; faces: FontFaceSource[]; }
export interface FontManifest { version: 1; files: string[]; bindings: FontBinding[]; }

export function readFontManifestData(manifestPath?: string): FontManifest {
  const empty: FontManifest = { version: 1, files: [], bindings: [] };
  if (!manifestPath || !fs.existsSync(manifestPath)) return empty;
  const text = fs.readFileSync(manifestPath, 'utf8').trim();
  if (!text.startsWith('{')) {
    return { ...empty, files: text.split('\n').map(s => s.trim()).filter(s => s && fs.existsSync(s)) };
  }
  const data = JSON.parse(text);
  if (data.version !== 1 || !Array.isArray(data.files) || !Array.isArray(data.bindings)) {
    throw new Error(`Unsupported font manifest: ${manifestPath}`);
  }
  const resolve = (file: string) => path.resolve(path.dirname(manifestPath), file);
  data.files = data.files.map(resolve);
  for (const binding of data.bindings) {
    if (typeof binding.family !== 'string' || !Array.isArray(binding.faces)) throw new Error('Invalid font binding');
    for (const face of binding.faces) {
      if (face.path) face.path = resolve(face.path);
      if (!face.path && !face.systemFamily) throw new Error('Font face has no source');
      if (face.faceIndex !== undefined && (!Number.isInteger(face.faceIndex) || face.faceIndex < 0)) {
        throw new Error('Invalid font face index');
      }
    }
  }
  return data;
}

// Expand CSS U+ ranges to a strict numeric protocol. Empty means the full Unicode range.
export function encodeUnicodeRanges(value = ''): string {
  if (!value) return '';
  return value.split(',').map(token => {
    const match = /^\s*U\+([0-9a-f?]{1,6})(?:-([0-9a-f]{1,6}))?\s*$/i.exec(token);
    if (!match || (match[2] && match[1].includes('?'))) throw new Error(`Invalid unicode-range: ${value}`);
    const lo = parseInt(match[1].replace(/\?/g, '0'), 16);
    const hi = parseInt(match[2] || match[1].replace(/\?/g, 'f'), 16);
    if (lo > hi || hi > 0x10ffff) throw new Error(`Invalid unicode-range: ${value}`);
    return `${lo}-${hi}`;
  }).join(',');
}

export function fontManifestArgs(manifestPath?: string): string[] {
  const data = readFontManifestData(manifestPath);
  if (!data.bindings.length) return data.files.flatMap(file => ['--fallback', file]);
  return data.bindings.flatMap(binding => binding.faces.flatMap(face => [
    '--font-face', binding.family, face.path || face.systemFamily!,
    String(face.path ? (face.faceIndex || 0) : -1), face.style || 'Regular',
    encodeUnicodeRanges(face.unicodeRange),
  ]));
}
