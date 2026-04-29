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

#include "pagx/PAGXOptimizer.h"

namespace pagx {

// INTERNAL: fine-grained optimization options used by unit tests and CLI debug commands.
// Not part of the public API — callers should use PAGXOptimizer::Optimize(doc) instead.
struct PAGXOptimizerOptions {
  /** Remove empty Layers and empty Groups. */
  bool pruneEmpty = true;
  /** When a parent Layer has only shell children, convert children to inline Groups. */
  bool downgradeShellChildren = true;
  /** Merge runs of adjacent shell Layers into a single Layer with one Group per source. */
  bool mergeAdjacentShellLayers = true;
  /** Unwrap a leading Group child when it has default transform and no painter isolation needed. */
  bool unwrapRedundantFirstGroup = true;
  /** Collapse consecutive Groups with identical painters into a single Group. */
  bool mergeAdjacentGroups = true;
  /** Rewrite Path nodes that describe an axis-aligned rectangle or an ellipse. */
  bool canonicalizePaths = true;
  /** Replace alpha-rectangle masks with an equivalent scrollRect on the parent layer. */
  bool rectMaskToScrollRect = true;
  /** Collapse PathData resources with identical contents into a single instance. */
  bool dedupPathData = true;
  /** Drop resources from `<Resources>` that no longer have any reference in the layer tree. */
  bool pruneUnreferencedResources = true;
  /**
   * Maximum number of iterations to run rules to a fixed point. In typical SVG import workloads
   * 2-3 iterations suffice; 8 is a generous upper bound to handle deeply nested structures.
   */
  int maxIterations = 8;
};

// Runs the optimizer with explicit options. Intended for unit tests and CLI debug commands.
PAGXOptimizer::Result OptimizeWithOptions(PAGXDocument* doc, const PAGXOptimizerOptions& options);

}  // namespace pagx
