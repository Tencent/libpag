#!/usr/bin/env node
// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
require('./ensure-built').ensureBuilt();
const { readImportRequest, runImportRequest, importErrorMessage } = require('./dist/lib/import-request');
(async () => {
  try {
    process.exitCode = await runImportRequest(readImportRequest(process.argv.slice(2)), __dirname);
  } catch (error) {
    process.stderr.write(`pagx import: ${importErrorMessage(error)}\n`);
    process.exitCode = error.exitCode || (Number.isInteger(error.code) && error.code > 0 ? error.code : 1);
  }
})();
