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
 * Check layout structure and detect layout problems in PAGX files.
 *
 * Detects four categories of layout problems:
 * 1. Overlapping sibling elements
 * 2. Content clipped by parent bounds (clipToBounds)
 * 3. Elements excluded from layout flow inside auto-layout parents (includeInLayout=false)
 * 4. Zero-size (invisible) elements
 *
 * Returns non-zero exit code when layout problems are detected.
 */
int RunLayout(int argc, char* argv[]);

}  // namespace pagx::cli
