#!/usr/bin/env node
// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
require('./runtime').launchBundled('capture').then(code => { process.exitCode = code; });
