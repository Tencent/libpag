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
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include "tgfx/core/Image.h"
#include "tgfx/core/Rect.h"
#include "tgfx/layers/filters/LayerFilter.h"

namespace tgfx {
struct Point;
}

namespace pagx {

// Layer filter for the PAGX GlowFilter node (spec 7.5). Replicates the legacy GlowFilter::Apply
// pipeline (src/rendering/filters/glow/): a fixed 5-tap gaussian blur pass per axis over a
// slightly downscaled copy, then a merge pass that brightens the source by max(src, blur) and
// divides by the threshold. The blur and merge reuse the legacy RuntimeFilter shaders so both
// pipelines stay pixel-identical; they run cache-less because the PAGX layer path has no
// pag::RenderCache. Alpha is preserved by the merge shader, so the glow never extends beyond
// the original content silhouette and the filter bounds stay unchanged.
class GlowLayerFilter : public tgfx::LayerFilter {
 public:
  static std::shared_ptr<GlowLayerFilter> Make(float glowThreshold, float glowRadius,
                                               float glowIntensity);

  float glowThreshold() const {
    return _glowThreshold;
  }

  void setGlowThreshold(float value);

  float glowRadius() const {
    return _glowRadius;
  }

  void setGlowRadius(float value);

  float glowIntensity() const {
    return _glowIntensity;
  }

  void setGlowIntensity(float value);

 protected:
  std::shared_ptr<tgfx::Image> onFilterImage(std::shared_ptr<tgfx::Image> input, float scale,
                                             const tgfx::Rect& contentBounds,
                                             tgfx::Point* offset) override;

 private:
  float _glowThreshold = 0.0f;
  float _glowRadius = 0.0f;
  float _glowIntensity = 1.0f;
};

}  // namespace pagx
