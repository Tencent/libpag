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
 * PAGXOptimizer simplifies the structure of a freshly imported PAGXDocument by collapsing
 * redundant Layers and Groups, merging adjacent shell Layers, and rewriting Path nodes that
 * actually describe primitive shapes (Rectangle / Ellipse). The optimizer is format-agnostic:
 * external importers (e.g. SVGImporter) produce a literal Layer/Group tree first, and the
 * optimizer then reduces it to a minimal equivalent representation.
 *
 * The optimizer is conservative: every rewrite preserves rendering output exactly, painter
 * scoping is never broken, mask references and customData attributes are preserved, and any
 * node that participates in layout (constraints, layout container) is left untouched.
 *
 * The optimizer does NOT reset layout state. Run it before applyLayout() (and before
 * FontEmbedder()) so the cached layout / embedded fonts are not invalidated.
 */
class PAGXOptimizer {
 public:
  struct Result {
    /**
     * True when the rule set reached a fixed point within the iteration cap. A false value means
     * the optimizer was cut off, which either indicates a rule set that is still making progress or
     * (rarely) two rules oscillating against each other.
     */
    bool converged = true;
    /**
     * Number of rule iterations actually executed (both on the root layer list and on each
     * Composition's layer list). Useful as a monotonic signal for regression / telemetry.
     */
    int iterationsUsed = 0;
  };

  /**
   * Simplifies the structure of `doc` in place. Has no effect on rendering output.
   * Safe to call multiple times; the second call is a no-op once the document is already
   * at a fixed point. When the optimizer fails to converge, a warning is appended to
   * `doc->errors` and `Result::converged` is set to false.
   */
  static Result Optimize(PAGXDocument* doc);
};

}  // namespace pagx
