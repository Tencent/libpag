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

// Layer filter for the PAGX MosaicFilter node (spec 7.5). Reuses the legacy mosaic RuntimeFilter
// shader (src/rendering/filters/MosaicFilter.cpp) so both pipelines quantize to identical block
// centers; it runs cache-less because the PAGX layer path has no pag::RenderCache. The shader
// takes the block SIZE in uv space, so the block counts invert here exactly like the legacy
// MosaicFilter::Apply did. The filter never changes bounds.
class MosaicLayerFilter : public tgfx::LayerFilter {
 public:
  static std::shared_ptr<MosaicLayerFilter> Make(float horizontalBlocks, float verticalBlocks,
                                                 bool sharpColors);

  float horizontalBlocks() const {
    return _horizontalBlocks;
  }

  void setHorizontalBlocks(float value);

  float verticalBlocks() const {
    return _verticalBlocks;
  }

  void setVerticalBlocks(float value);

  bool sharpColors() const {
    return _sharpColors;
  }

  void setSharpColors(bool value);

 protected:
  std::shared_ptr<tgfx::Image> onFilterImage(std::shared_ptr<tgfx::Image> input, float scale,
                                             const tgfx::Rect& contentBounds, tgfx::Point* offset,
                                             const tgfx::Point& inputOrigin) override;

 private:
  float _horizontalBlocks = 1.0f;
  float _verticalBlocks = 1.0f;
  bool _sharpColors = false;
};

}  // namespace pagx
