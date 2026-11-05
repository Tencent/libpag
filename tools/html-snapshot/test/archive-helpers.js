// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const fs = require('fs');
const { pipeline } = require('stream/promises');
const { ZipFile } = require('yazl');

async function writeZip(file, entries) {
  const zip = new ZipFile();
  const done = pipeline(zip.outputStream, fs.createWriteStream(file));
  for (const [name, value] of Object.entries(entries)) {
    if (name.endsWith('/')) zip.addEmptyDirectory(name);
    else {
      const data = value && typeof value === 'object' && !Buffer.isBuffer(value) ? value : { data: value };
      zip.addBuffer(Buffer.from(data.data), name, { compress: false, ...data.options });
    }
  }
  zip.end();
  await done;
  return file;
}

module.exports = { writeZip };
