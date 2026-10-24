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
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "pagx/PAGExporter.h"
#include "pagx/PAGXDocument.h"

namespace pagx {

// One ImageTable entry per the binary spec §6.6. The exporter produces these; the encoder
// writes them sequentially, and the decoder hands each Image node a zero-copy Data slice.
struct ImageTableEntry {
  // 0 = static WebP, 1 = raw bitmap glyph (spec §6.6). Phase 1 only produces 0/1.
  uint8_t mediaType = 0;
  std::shared_ptr<Data> data = {};
  uint32_t logicalWidth = 0;
  uint32_t logicalHeight = 0;
  float scaleFactor = 1.0f;
  float offsetX = 0.0f;
  float offsetY = 0.0f;
};

// Export-time image processing (design doc §4.5). Runs after layout flattening (T2) and
// stripping (T3). Walks the document, computes each image's maximum display scale, and
// produces the ImageTable entries with scaled, cropped, and re-encoded image data.
class ImageProcessor {
 public:
  struct Options {
    // §12.2 phase 2: the maximum display scale must include keyframed transforms. The lookup
    // receives a transform node (Layer, Group/TextBox, ImagePattern) and returns the
    // animation-driven scale multiplier upper bound (>= 1) — the largest keyframe scale over
    // the node's static one. Null treats every animation as 1 (phase 1 behavior).
    std::function<float(Node* node)> animationScaleLookup = nullptr;
  };

  struct Result {
    std::vector<ImageTableEntry> entries = {};
    // Maps each Image node to its entry index in the entries vector (first-reference order).
    std::unordered_map<const Image*, uint32_t> imageIndices = {};
    // Warnings produced during processing.
    std::vector<PAGExportIssue> warnings = {};
  };

  /**
   * Processes all Image nodes in the document: computes max display scale per image,
   * scales down (factor = min(1, scale × 2.0)), crops transparent edges when every
   * reference uses decal tiling, encodes as WebP q80, deduplicates identical content, and
   * handles missing images with a warning.
   */
  static Result Process(PAGXDocument* document, const Options* options = nullptr);

  /**
   * Computes the maximum display scale of an image: the largest singular value of the
   * cumulative ancestor transform at any ImagePattern that references the image, combined
   * with the pattern's own matrix and scaleMode fitting.
   */
  static float ComputeMaxDisplayScale(PAGXDocument* document, const Image* image,
                                      const Options* options = nullptr);
};

}  // namespace pagx
