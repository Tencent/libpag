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
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "renderer/OuterGlowLayerFilter.h"
#include <utility>
#include "rendering/filters/layerstyle/SolidStrokeFilter.h"
#include "rendering/filters/utils/BlurTypes.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/Paint.h"
#include "tgfx/core/PictureRecorder.h"

namespace pagx {

void OuterGlowLayerFilter::setColor(tgfx::Color value) {
  setOuterGlowField(_color, value);
}

void OuterGlowLayerFilter::setSize(float value) {
  setOuterGlowField(_size, value);
}

void OuterGlowLayerFilter::setAlpha(float value) {
  setOuterGlowField(_alpha, value);
}

void OuterGlowLayerFilter::setSpread(float value) {
  setOuterGlowField(_spread, value);
}

void OuterGlowLayerFilter::setRange(float value) {
  setOuterGlowField(_range, value);
}

void OuterGlowLayerFilter::setBlendMode(tgfx::BlendMode value) {
  if (_blendMode == value) {
    return;
  }
  _blendMode = value;
  invalidateFilter();
}

std::shared_ptr<tgfx::ImageFilter> OuterGlowLayerFilter::makeHaloFilter(float scale) const {
  if (_size <= 0.0f) {
    return nullptr;
  }
  // The legacy divides every extent by the range percent (default 50%), so a tighter range
  // stretches the same size further out from the contour.
  auto range = _range / 100.0f;
  auto spread = _spread / 100.0f;
  if (range <= 0.0f) {
    return nullptr;
  }
  auto sizeX = _size * scale;
  auto sizeY = _size * scale;
  auto blurSigmaX = sizeX * (1.0f - spread) / range / 2.0f;
  auto blurSigmaY = sizeY * (1.0f - spread) / range / 2.0f;
  if (spread <= 0.0f) {
    return tgfx::ImageFilter::DropShadowOnly(0.0f, 0.0f, blurSigmaX, blurSigmaY, _color);
  }
  // The stroke mode switches on the raw (layer-unit) size, matching the legacy update(); the
  // stroke keeps the Invalid position so it paints a pure dilated silhouette.
  auto mode = _size < STROKE_SPREAD_MIN_THICK_SIZE ? pag::SolidStrokeMode::Normal
                                                   : pag::SolidStrokeMode::Thick;
  auto strokeOption = pag::SolidStrokeOption();
  strokeOption.color = _color;
  strokeOption.spreadSizeX = spread * sizeX / range;
  strokeOption.spreadSizeY = spread * sizeY / range;
  auto strokeFilter = pag::SolidStrokeFilter::CreateFilter(nullptr, strokeOption, mode);
  if (spread >= 1.0f) {
    return strokeFilter;
  }
  if (strokeFilter == nullptr) {
    return nullptr;
  }
  return tgfx::ImageFilter::Compose(
      std::move(strokeFilter),
      tgfx::ImageFilter::DropShadowOnly(0.0f, 0.0f, blurSigmaX, blurSigmaY, _color));
}

std::shared_ptr<tgfx::Image> OuterGlowLayerFilter::onFilterImage(std::shared_ptr<tgfx::Image> input,
                                                                 float scale, const tgfx::Rect&,
                                                                 tgfx::Point* offset,
                                                                 const tgfx::Point&) {
  if (input == nullptr || _alpha <= 0.0f) {
    return input;
  }
  auto filter = makeHaloFilter(scale);
  if (filter == nullptr) {
    return input;
  }
  tgfx::Point haloOffset = {};
  auto haloImage = input->makeWithFilter(std::move(filter), &haloOffset);
  if (haloImage == nullptr || haloImage == input) {
    return input;
  }
  // Composite like the legacy LayerStylesFilter::applyFilter(): the blow-position halo draws
  // first (its blend mode is inert against the empty recording) and the source image last.
  tgfx::PictureRecorder recorder = {};
  auto* canvas = recorder.beginRecording();
  tgfx::Paint paint = {};
  paint.setAlpha(_alpha);
  paint.setBlendMode(_blendMode);
  canvas->drawImage(haloImage, 0.0f, 0.0f, &paint);
  canvas->drawImage(input, -haloOffset.x, -haloOffset.y);
  auto picture = recorder.finishRecordingAsPicture();
  if (picture == nullptr) {
    return input;
  }
  auto result = tgfx::Image::MakeFrom(std::move(picture), haloImage->width(), haloImage->height());
  if (result == nullptr) {
    return input;
  }
  if (offset != nullptr) {
    *offset = haloOffset;
  }
  return result;
}

tgfx::Rect OuterGlowLayerFilter::filterBounds(const tgfx::Rect& srcRect, float contentScale,
                                              tgfx::MapDirection direction) {
  if (srcRect.isEmpty()) {
    return srcRect;
  }
  auto filter = makeHaloFilter(contentScale);
  if (filter == nullptr) {
    return srcRect;
  }
  return filter->filterBounds(srcRect, direction);
}

}  // namespace pagx
