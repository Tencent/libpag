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

async function readZip(file) {
  const zip = await require('yauzl').openPromise(file);
  const entries = new Map();
  for await (const entry of zip.eachEntry()) {
    const chunks = [];
    for await (const chunk of await zip.openReadStreamPromise(entry)) chunks.push(chunk);
    entries.set(entry.fileName, Buffer.concat(chunks));
  }
  return entries;
}

module.exports = { writeZip, readZip };
