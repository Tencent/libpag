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

#pragma once

namespace pagx::cli {

/**
 * Validates, optimizes, and formats a PAGX file in one step. Validates input against the XSD
 * schema first — aborts with errors if invalid. Then applies deterministic structural
 * optimizations and exports with consistent formatting. There is no need to run
 * `pagx validate` or `pagx format` separately after optimize.
 */
int RunOptimize(int argc, char* argv[]);

}  // namespace pagx::cli
