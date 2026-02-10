#!/usr/bin/env node
/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

import { fileURLToPath } from 'url';
import { createRequire } from 'module';
import path from 'path';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const require = createRequire(import.meta.url);

const playgroundDir = path.dirname(__dirname);
process.chdir(playgroundDir);

process.argv.push("-s");
process.argv.push("./");
process.argv.push("-o");
process.argv.push("./");
process.argv.push("-p");
process.argv.push("web");
process.argv.push("pagx-playground");

// Use vendor_tools from libpag
require("../../third_party/vendor_tools/lib-build");