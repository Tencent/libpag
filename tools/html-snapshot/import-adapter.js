#!/usr/bin/env node
// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
require('./ensure-built').ensureBuilt();
const fs = require('fs');
const { withCliLifecycle } = require('./dist/lib/cli-lifecycle');
const { runImportAdapter } = require('./dist/lib/import-adapter');
withCliLifecycle(async () => {
  if (process.argv.length !== 4) throw new Error('internal import adapter expects request and response paths');
  return runImportAdapter(JSON.parse(fs.readFileSync(process.argv[2], 'utf8')), process.argv[3]);
}).then(code => { process.exitCode = code; }).catch(error => {
  process.stderr.write(`pagx import: ${error.message}\n`);
  process.exitCode = error.exitCode || 1;
});
