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
  struct Options {
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
    /**
     * Split Path elements whose PathData verb count exceeds `pathVerbThreshold` into multiple
     * sibling Paths at sub-path boundaries. Hole sub-paths (bounding-box-contained) are kept
     * together with their outer contour so fill-rule rendering is preserved exactly. Skipped
     * for reversed paths, paths with layout constraints or customData, and Paths whose
     * PathData is a shared/named resource.
     *
     * Off by default because the resulting Paths each carry their own PathData and slightly
     * inflate the document; enable when targeting a renderer that warns on high-verb paths.
     */
    bool splitLongPaths = false;
    /** Verb count above which a Path becomes eligible for splitting. Matches the warning
     * threshold used by `pagx verify` (`> 1024`). */
    int pathVerbThreshold = 1024;
    /** Maximum verbs allowed in an emitted chunk; must be strictly less than the threshold. */
    int maxVerbsPerPath = 900;
    /**
     * Allow the splitter to chord-split a single closed sub-path that exceeds the threshold
     * by inserting an internal straight chord, producing two filled halves whose union
     * matches the original area. The chord runs inside the contour so it is invisible for
     * solid fills but may be perceptible on stroked or alpha-composited paths. Off by
     * default — opt in for assets where you have verified the affected paths are filled.
     */
    bool enableChordSplit = false;
    /** Collapse PathData resources with identical contents into a single instance. */
    bool dedupPathData = true;
    /** Drop resources from `<Resources>` that no longer have any reference in the layer tree. */
    bool pruneUnreferencedResources = true;
    /** Maximum number of iterations to run rules to a fixed point. */
    int maxIterations = 8;

    Options() {
    }
  };

  /**
   * Simplifies the structure of `doc` in place. Has no effect on rendering output.
   * Safe to call multiple times; the second call is a no-op once the document is already
   * at a fixed point.
   */
  static void Optimize(PAGXDocument* doc, const Options& options = Options());
};

}  // namespace pagx
