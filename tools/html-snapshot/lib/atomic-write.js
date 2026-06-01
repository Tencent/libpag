// Atomic single-file writer shared by lib/font-download.js and
// lib/image-download.js. Both modules write content-addressed files into a
// directory that may be shared across parallel workers (eval/run.js processes
// many cases in parallel into one shared font/image directory), so a writer
// must never expose a half-written file to a concurrent reader.
//
// Strategy: stage the bytes in a per-call temp file (`<filePath>.tmp-<pid>-<rnd>`)
// and atomically rename it into place. If another worker won the race and
// created the (byte-identical, content-addressed) target first, discard our
// temp and treat the conflict as success.

'use strict';

const fs = require('fs');
const fsp = require('fs').promises;
const crypto = require('crypto');

async function writeFileAtomic(filePath, data) {
  const tmp = `${filePath}.tmp-${process.pid}-${crypto.randomBytes(4).toString('hex')}`;
  await fsp.writeFile(tmp, data);
  try {
    await fsp.rename(tmp, filePath);
  } catch (err) {
    await fsp.unlink(tmp).catch(() => {});
    if (!fs.existsSync(filePath)) throw err;
  }
}

module.exports = { writeFileAtomic };
