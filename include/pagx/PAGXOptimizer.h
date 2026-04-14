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

#include "pagx/PAGXDocument.h"

namespace pagx {

/**
 * PAGXOptimizer simplifies a PAGXDocument's structure in-place without changing its visual output.
 * Typical transformations include:
 * - Downgrading child Layers to Groups when no Layer-exclusive features are used
 * - Merging consecutive Groups with identical painter prefixes and default transforms
 * - Unwrapping redundant first-child Groups
 * - Removing empty Layers and Groups
 *
 * This pass is format-agnostic: it operates on the PAGXDocument data model only, so every import
 * format (SVG, Lottie, etc.) benefits from the same simplification.
 */
class PAGXOptimizer {
 public:
  /**
   * Optimizes the document structure in-place. Returns true if any modifications were made.
   * The caller must NOT have called applyLayout() before this — the optimizer works on the raw
   * document tree and the document should be re-exported / re-laid-out after optimization.
   */
  static bool Optimize(PAGXDocument* document);
};

}  // namespace pagx
