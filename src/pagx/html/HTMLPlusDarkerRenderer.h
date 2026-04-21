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

#include <string>
#include <unordered_map>

namespace pagx {

class Layer;
class PAGXDocument;

/**
 * Per-plusDarker-Layer backdrop data consumed by HTMLWriter to emit an SVG filter whose feImage
 * supplies the pre-composited backdrop pixels and feComposite arithmetic computes
 * clamp(Sc + Dc - 1, 0, 1), matching tgfx PlusDarker semantics pixel-for-pixel.
 */
struct PlusDarkerBackdrop {
  std::string filterId;         // e.g. "pd_0"
  std::string backdropDataURL;  // "data:image/png;base64,..."
  float cropLeft = 0;           // screen-space CSS px
  float cropTop = 0;
  float cropWidth = 0;  // CSS px
  float cropHeight = 0;
};

/**
 * Runs a pre-pass over a PAGXDocument, rendering a cropped backdrop PNG for every Layer whose
 * blendMode is PlusDarker and whose structure is compatible with the filter-based path. The
 * produced base64 data URLs are embedded into the generated HTML as feImage sources so the
 * browser composites the exact backdrop tgfx sees.
 */
class HTMLPlusDarkerRenderer {
 public:
  /**
   * For every compatible PlusDarker Layer found in doc, writes a backdrop PNG into staticImgDir
   * and registers a PlusDarkerBackdrop keyed by the Layer pointer. Layers that fail the
   * compatibility check (filters, masks, 3D transforms, non-translation matrix, scroll rect) are
   * silently skipped and the caller falls back to the mix-blend-mode: darken approximation.
   *
   * Temporarily flips layer->visible via const_cast to achieve "render doc minus layer"; the
   * value is restored on every code path including early returns. The caller must not use the
   * document concurrently during this call.
   */
  static void RenderAll(const PAGXDocument& doc, const std::string& staticImgDir,
                        const std::string& staticImgUrlPrefix,
                        const std::string& staticImgNamePrefix, float pixelRatio,
                        std::unordered_map<const Layer*, PlusDarkerBackdrop>& out);
};

}  // namespace pagx
