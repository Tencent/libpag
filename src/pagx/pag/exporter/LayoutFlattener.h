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
//  or required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <string>
#include <vector>
#include "pagx/PAGExporter.h"
#include "pagx/nodes/Node.h"

namespace pagx {

class PAGXDocument;

// Options for the export-time layout flattening (design doc §4.2).
struct FlattenOptions {
  // §12.2 hook: invoked when a uniform layout scale is baked into a node's geometry
  // (Path, TextPath, Polystar). Phase 1 has no keyframes to transform and leaves this
  // unset; phase 2 assigns it to rescale per-point and path channel keyframes.
  std::function<void(Node* node, float scale)> geometryScaled = nullptr;
};

/**
 * Writes the solved layout results back to explicit properties and removes every layout
 * input, turning an applyLayout()-solved document into a flat one that re-measures to the
 * same picture (design doc §4.2). Destructive: operates in place on the given document.
 * The document must already be laid out; if it is not, it is laid out with its current font
 * config first, so the flattener never reads stale values.
 */
class LayoutFlattener {
 public:
  /**
   * Flattens the whole document: root layers, resource compositions, and every nested layer
   * and vector element. Sub-documents are not touched here; the exporter flattens each one
   * on its own (design doc §4.8).
   */
  static void Flatten(PAGXDocument* document, const FlattenOptions& options = {});

  /**
   * Returns true when the channel exists, requires a layout pass, and can never be animated
   * (design doc §4.2): animations and data binds targeting it are inert in the XML runtime
   * too, so the exporter drops them with a LayoutChannelDropped warning.
   */
  static bool IsLayoutOnlyChannel(NodeType type, const std::string& channel);

  /**
   * Collects a LayoutChannelDropped issue for every animation channel and data bind in the
   * document that targets a layout-only channel, including composition-scoped ones. Targets
   * that cannot be resolved are skipped; the runtime drops them silently.
   */
  static void CollectLayoutChannelIssues(PAGXDocument* document, const std::string& scope,
                                         std::vector<PAGExportIssue>* issues);
};

}  // namespace pagx
