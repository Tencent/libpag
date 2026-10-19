#!/usr/bin/env node
// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
require('./ensure-built').ensureBuilt();
const { runCaptureCli } = require('./capture-cli');
runCaptureCli(process.argv.slice(2), { toolName: 'pagx capture' })
  .then(code => { process.exitCode = code; });
