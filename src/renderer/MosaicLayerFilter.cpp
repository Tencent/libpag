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

#include "MosaicLayerFilter.h"
#include <utility>
#include "rendering/filters/MosaicFilter.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/Point.h"

namespace pagx {

std::shared_ptr<MosaicLayerFilter> MosaicLayerFilter::Make(float horizontalBlocks,
                                                           float verticalBlocks, bool sharpColors) {
  auto filter = std::shared_ptr<MosaicLayerFilter>(new MosaicLayerFilter());
  filter->_horizontalBlocks = horizontalBlocks;
  filter->_verticalBlocks = verticalBlocks;
  filter->_sharpColors = sharpColors;
  return filter;
}

void MosaicLayerFilter::setHorizontalBlocks(float value) {
  if (_horizontalBlocks == value) {
    return;
  }
  _horizontalBlocks = value;
  invalidateFilter();
}

void MosaicLayerFilter::setVerticalBlocks(float value) {
  if (_verticalBlocks == value) {
    return;
  }
  _verticalBlocks = value;
  invalidateFilter();
}

void MosaicLayerFilter::setSharpColors(bool value) {
  if (_sharpColors == value) {
    return;
  }
  _sharpColors = value;
  invalidateFilter();
}

std::shared_ptr<tgfx::Image> MosaicLayerFilter::onFilterImage(std::shared_ptr<tgfx::Image> input,
                                                              float, const tgfx::Rect&,
                                                              tgfx::Point* offset) {
  if (input == nullptr) {
    return nullptr;
  }
  // Mirrors the legacy MosaicFilter::Apply math: the RuntimeFilter shader quantizes uv space,
  // so the block counts invert into block sizes before handing over.
  auto filter = std::make_shared<pag::MosaicFilter>(nullptr, 1.0f / _horizontalBlocks,
                                                    1.0f / _verticalBlocks, _sharpColors);
  return input->makeWithFilter(tgfx::ImageFilter::Runtime(std::move(filter)), offset);
}

}  // namespace pagx
