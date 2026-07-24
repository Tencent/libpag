// Atomic single-file writer shared by lib/font-download.ts and
// lib/image-download.ts. Both modules write content-addressed files into a
// directory that may be shared across parallel workers (eval/run.js processes
// many cases in parallel into one shared font/image directory), so a writer
// must never expose a half-written file to a concurrent reader.
//
// Strategy: stage the bytes in a per-call temp file (`<filePath>.tmp-<pid>-<rnd>`)
// and atomically rename it into place. If another worker won the race and
// created the (byte-identical, content-addressed) target first, discard our
// temp and treat the conflict as success.

import * as fs from 'fs';
import * as crypto from 'crypto';

const fsp = fs.promises;

export async function writeFileAtomic(
  filePath: string,
  data: Buffer | Uint8Array | string,
): Promise<void> {
  const tmp = `${filePath}.tmp-${process.pid}-${crypto.randomBytes(4).toString('hex')}`;
  await fsp.writeFile(tmp, data);
  try {
    await fsp.rename(tmp, filePath);
  } catch (err) {
    await fsp.unlink(tmp).catch(() => {});
    if (!fs.existsSync(filePath)) throw err;
  }
}
