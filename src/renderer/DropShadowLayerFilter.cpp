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

#include "renderer/DropShadowLayerFilter.h"
#include <utility>
#include "rendering/filters/layerstyle/SolidStrokeFilter.h"
#include "rendering/filters/utils/BlurTypes.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/Paint.h"
#include "tgfx/core/PictureRecorder.h"

namespace pagx {

void DropShadowLayerFilter::setOffsetX(float value) {
  setDropShadowField(_offsetX, value);
}

void DropShadowLayerFilter::setOffsetY(float value) {
  setDropShadowField(_offsetY, value);
}

void DropShadowLayerFilter::setBlurX(float value) {
  setDropShadowField(_blurX, value);
}

void DropShadowLayerFilter::setBlurY(float value) {
  setDropShadowField(_blurY, value);
}

void DropShadowLayerFilter::setSpread(float value) {
  setDropShadowField(_spread, value);
}

void DropShadowLayerFilter::setColor(tgfx::Color value) {
  setDropShadowField(_color, value);
}

void DropShadowLayerFilter::setBlendMode(tgfx::BlendMode value) {
  if (_blendMode == value) {
    return;
  }
  _blendMode = value;
  invalidateFilter();
}

void DropShadowLayerFilter::setShowBehindLayer(bool value) {
  if (_showBehindLayer == value) {
    return;
  }
  _showBehindLayer = value;
  invalidateFilter();
}

std::shared_ptr<tgfx::ImageFilter> DropShadowLayerFilter::makeShadowFilter(float scale) const {
  auto spread = _spread / 100.0f;
  if (spread <= 0.0f) {
    return tgfx::ImageFilter::DropShadowOnly(_offsetX * scale, _offsetY * scale, _blurX * scale,
                                             _blurY * scale, _color);
  }
  if (_blurX <= 0.0f && _blurY <= 0.0f) {
    return nullptr;
  }
  // The node stores the pre-spread blur radius (half the legacy size); the spread converts part
  // of it into the solid core exactly like the legacy AE split: effective sigma = blur * (1 - s),
  // core = 2 * blur * s.
  auto coreX = _blurX * 2.0f * spread;
  auto coreY = _blurY * 2.0f * spread;
  auto sigmaX = _blurX * (1.0f - spread);
  auto sigmaY = _blurY * (1.0f - spread);
  // The stroke mode switches on the unscaled core in layer units, matching the legacy update()
  // (size * spread < the thick threshold).
  auto mode = coreX < STROKE_SPREAD_MIN_THICK_SIZE ? pag::SolidStrokeMode::Normal
                                                   : pag::SolidStrokeMode::Thick;
  auto option = pag::SolidStrokeOption();
  option.color = _color;
  option.spreadSizeX = coreX * scale;
  option.spreadSizeY = coreY * scale;
  // The mixed spread case dilates at the shadow offset and blurs without one, matching the
  // legacy compose order.
  option.offsetX = _offsetX * scale;
  option.offsetY = _offsetY * scale;
  auto strokeFilter = pag::SolidStrokeFilter::CreateFilter(nullptr, option, mode);
  if (strokeFilter == nullptr) {
    return nullptr;
  }
  if (spread >= 1.0f) {
    return strokeFilter;
  }
  return tgfx::ImageFilter::Compose(
      std::move(strokeFilter),
      tgfx::ImageFilter::DropShadowOnly(0.0f, 0.0f, sigmaX * scale, sigmaY * scale, _color));
}

std::shared_ptr<tgfx::Image> DropShadowLayerFilter::onFilterImage(
    std::shared_ptr<tgfx::Image> input, float scale, const tgfx::Rect&, tgfx::Point* offset,
    const tgfx::Point&) {
  if (input == nullptr) {
    return input;
  }
  auto filter = makeShadowFilter(scale);
  if (filter == nullptr) {
    return input;
  }
  tgfx::Point shadowOffset = {};
  auto shadowImage = input->makeWithFilter(std::move(filter), &shadowOffset);
  if (shadowImage == nullptr || shadowImage == input) {
    return input;
  }
  // Composite like the legacy LayerStylesFilter::applyFilter(): the blow-position shadow draws
  // first and the source image last. The style blend mode is NOT applied: the legacy renderer
  // ignores it (only the gradient overlay reads it), and blends like Multiply would erase the
  // shadow against the empty recording entirely.
  tgfx::PictureRecorder recorder = {};
  auto* canvas = recorder.beginRecording();
  canvas->drawImage(shadowImage, 0.0f, 0.0f);
  if (!_showBehindLayer) {
    // Knock the shadow out of the content's coverage: the tgfx native style masks by the
    // layer's vector contour, which this filter path cannot extract, so the alpha-weighted
    // erase approximates it (identical for opaque content).
    tgfx::Paint erase = {};
    erase.setBlendMode(tgfx::BlendMode::DstOut);
    canvas->drawImage(input, -shadowOffset.x, -shadowOffset.y, &erase);
  }
  canvas->drawImage(input, -shadowOffset.x, -shadowOffset.y);
  auto picture = recorder.finishRecordingAsPicture();
  if (picture == nullptr) {
    return input;
  }
  auto result =
      tgfx::Image::MakeFrom(std::move(picture), shadowImage->width(), shadowImage->height());
  if (result == nullptr) {
    return input;
  }
  if (offset != nullptr) {
    *offset = shadowOffset;
  }
  return result;
}

tgfx::Rect DropShadowLayerFilter::filterBounds(const tgfx::Rect& srcRect, float contentScale,
                                               tgfx::MapDirection direction) {
  if (srcRect.isEmpty()) {
    return srcRect;
  }
  auto filter = makeShadowFilter(contentScale);
  if (filter == nullptr) {
    return srcRect;
  }
  return filter->filterBounds(srcRect, direction);
}

}  // namespace pagx
