// External <img> download → on-disk image export.
//
// By default the snapshot pipeline inlines every external image into a base64
// `data:` URI (see lib/snapshot-runner.ts + browser-snapshot.ts's
// `inlineExternalImages`), which makes the subset HTML — and the PAGX produced
// from it — self-contained but potentially huge: a page with a handful of
// full-resolution photos can balloon the document by several megabytes of
// base64.
//
// This module is the opt-in alternative, mirroring lib/font-download.ts: it
// saves the actual image *files* the browser already downloaded while rendering
// the page to a directory on disk, and the caller (snapshot-runner) then
// references each image by its local file path instead of inlining the bytes.
// PAGX's renderer can read local files directly (it only cannot fetch
// `http(s)://` URLs at render time), so the resulting document still renders
// correctly while staying small.
//
// Why capture the browser's downloads instead of re-fetching the URLs:
//   - The browser already has the decoded bytes; we read them off the response
//     and sidestep CORS on the image binaries (most image CDNs return no CORS
//     headers, which is why the legacy in-page `fetch()` path silently failed
//     for cross-origin images).
//
// Pipeline:
//   1. snapshot-runner registers an image-capture listener on `page` before
//      navigation; every `resourceType() === 'image'` response body is cached
//      by URL as `{ buffer, contentType }`.
//   2. After the page settles, `saveDownloadedImages` de-dupes by content hash,
//      picks an extension from the content-type (falling back to a magic-byte
//      sniff), writes each unique image into the requested directory with a
//      content-addressed filename, and returns the `url -> path` mapping the
//      caller hands to `inlineExternalImages`.

import * as fs from 'fs';
import * as path from 'path';
import * as crypto from 'crypto';
import { writeFileAtomic } from './atomic-write';
import { errMessage } from './common';

const fsp = fs.promises;

export interface ImageCaptureValue {
  buffer: Buffer;
  contentType?: string;
}

export type ImageLogger = (msg: string) => void;

export interface SavedImage {
  url: string;
  path: string;
}

// Map a response content-type (or a magic-byte sniff) to a file extension.
// PAGX matches images by decoding the file bytes, not the extension, so a
// wrong extension here only affects on-disk legibility, never rendering.
const CONTENT_TYPE_EXT: Record<string, string> = {
  'image/png': 'png',
  'image/jpeg': 'jpg',
  'image/jpg': 'jpg',
  'image/gif': 'gif',
  'image/webp': 'webp',
  'image/svg+xml': 'svg',
  'image/bmp': 'bmp',
  'image/x-icon': 'ico',
  'image/vnd.microsoft.icon': 'ico',
  'image/avif': 'avif',
  'image/tiff': 'tiff',
};

// Sniff the container from the leading bytes when the content-type is missing
// or generic (`application/octet-stream`). Covers the formats browsers
// actually emit for <img>; anything unrecognised falls back to `img`.
export function sniffExt(buf: Buffer | null | undefined): string {
  if (!buf || buf.length < 4) return 'img';
  const b = buf;
  if (b[0] === 0x89 && b[1] === 0x50 && b[2] === 0x4e && b[3] === 0x47) return 'png';
  if (b[0] === 0xff && b[1] === 0xd8 && b[2] === 0xff) return 'jpg';
  if (b[0] === 0x47 && b[1] === 0x49 && b[2] === 0x46) return 'gif';
  if (b[0] === 0x42 && b[1] === 0x4d) return 'bmp';
  if (b[0] === 0x00 && b[1] === 0x00 && b[2] === 0x01 && b[3] === 0x00) return 'ico';
  if (
    b.length >= 12 &&
    b[0] === 0x52 && b[1] === 0x49 && b[2] === 0x46 && b[3] === 0x46 &&
    b[8] === 0x57 && b[9] === 0x45 && b[10] === 0x42 && b[11] === 0x50
  ) {
    return 'webp';
  }
  // SVG is text; a leading `<?xml` or `<svg` prefix is a good-enough signal.
  const head = b.slice(0, 256).toString('utf8').trimStart().toLowerCase();
  if (head.startsWith('<?xml') || head.startsWith('<svg')) return 'svg';
  return 'img';
}

// Resolve the on-disk extension for a captured image, preferring the server's
// content-type (stripped of any `; charset=...` parameters) and falling back
// to a magic-byte sniff.
export function pickExt(contentType: string | undefined | null, buffer: Buffer): string {
  const ct = String(contentType || '').split(';')[0].trim().toLowerCase();
  if (CONTENT_TYPE_EXT[ct]) return CONTENT_TYPE_EXT[ct];
  return sniffExt(buffer);
}

// Filesystem-safe slug derived from the image URL's basename, so the saved
// file is recognisable on disk. Collapses any run of disallowed characters to
// a single dash and trims dashes; URLs that slug to empty (query-only, CJK)
// fall back to a stable default.
export function baseNameFromUrl(url: string): string {
  let name = '';
  try {
    const u = new URL(url);
    name = path.basename(u.pathname || '');
  } catch (_) {
    name = path.basename(String(url || '').split('?')[0]);
  }
  // Drop any existing extension; we append our own based on the content type.
  name = name.replace(/\.[A-Za-z0-9]+$/, '');
  const slug = name.replace(/[^A-Za-z0-9._-]+/g, '-').replace(/^-+|-+$/g, '');
  return slug || 'image';
}

// Build a content-addressed `<basename>-<hash>.<ext>` filename. The hash
// suffix (first 16 hex chars of the bytes' SHA-1) makes the name a pure
// function of the content, so the same image referenced by multiple <img>
// tags — or across separate snapshot runs sharing one image directory — always
// maps to the same filename and is stored once instead of accumulating copies.
function contentFileName(url: string, hash: string, ext: string): string {
  return `${baseNameFromUrl(url)}-${hash.slice(0, 16)}.${ext}`;
}

// Convert the captured `url -> { buffer, contentType }` map into on-disk image
// files under `outDir`. Returns `[{ url, path }]` for the unique images
// captured. De-dupes by content hash so the same image served from multiple
// URLs (or already-cached re-requests) is written once, and uses a
// content-addressed filename so an identical image already on disk — e.g.
// written by an earlier case sharing this directory — is reused rather than
// re-written. Per-image failures are logged and skipped — a single bad payload
// must not lose the rest.
export async function saveDownloadedImages(
  imageBytesByUrl: Map<string, ImageCaptureValue | Buffer> | null | undefined,
  outDir: string,
  logger?: ImageLogger,
): Promise<SavedImage[]> {
  const log: ImageLogger = typeof logger === 'function' ? logger : () => {};
  const entries = imageBytesByUrl ? Array.from(imageBytesByUrl.entries()) : [];
  if (entries.length === 0) return [];

  await fsp.mkdir(outDir, { recursive: true });

  const seenHashes = new Set<string>();
  const saved: SavedImage[] = [];
  for (const [url, value] of entries) {
    try {
      // Accept either the `{ buffer, contentType }` shape the capture listener
      // produces or a bare Buffer (used by tests / callers that don't track a
      // content-type). A Buffer exposes its own `.buffer` (the backing
      // ArrayBuffer), so check `Buffer.isBuffer` first to avoid unwrapping it.
      const buffer = Buffer.isBuffer(value)
        ? value
        : (value && (value as ImageCaptureValue).buffer);
      if (!Buffer.isBuffer(buffer) || !buffer.length) continue;
      const hash = crypto.createHash('sha1').update(buffer).digest('hex');
      if (seenHashes.has(hash)) continue;
      seenHashes.add(hash);
      const contentType = Buffer.isBuffer(value)
        ? ''
        : (value && (value as ImageCaptureValue).contentType);
      const ext = pickExt(contentType, buffer);
      const fileName = contentFileName(url, hash, ext);
      const filePath = path.join(outDir, fileName);
      if (!fs.existsSync(filePath)) {
        await writeFileAtomic(filePath, buffer);
      }
      saved.push({ url, path: filePath });
    } catch (err) {
      log(`failed to save image ${url}: ${errMessage(err)}`);
    }
  }
  return saved;
}
