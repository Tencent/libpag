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

#include "renderer/StrokeLayerFilter.h"
#include <utility>
#include "rendering/filters/layerstyle/AlphaEdgeDetectFilter.h"
#include "rendering/filters/layerstyle/SolidStrokeFilter.h"
#include "rendering/filters/utils/BlurTypes.h"
#include "tgfx/core/Canvas.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/Paint.h"
#include "tgfx/core/PictureRecorder.h"

namespace pagx {

void StrokeLayerFilter::setColor(tgfx::Color value) {
  setStrokeField(_color, value);
}

void StrokeLayerFilter::setSize(float value) {
  setStrokeField(_size, value);
}

void StrokeLayerFilter::setAlpha(float value) {
  setStrokeField(_alpha, value);
}

void StrokeLayerFilter::setPosition(StrokePosition value) {
  setStrokeField(_position, value);
}

void StrokeLayerFilter::setBlendMode(tgfx::BlendMode value) {
  if (_blendMode == value) {
    return;
  }
  _blendMode = value;
  invalidateFilter();
}

std::shared_ptr<tgfx::ImageFilter> StrokeLayerFilter::makeStrokeFilter(
    float scale, const std::shared_ptr<tgfx::Image>& original) const {
  if (_size <= 0.0f) {
    return nullptr;
  }
  // The stroke mode switches on the raw (layer-unit) size, matching the legacy update().
  auto mode = _size < STROKE_SPREAD_MIN_THICK_SIZE ? pag::SolidStrokeMode::Normal
                                                   : pag::SolidStrokeMode::Thick;
  auto option = pag::SolidStrokeOption();
  option.color = _color;
  option.spreadSizeX = _size * scale;
  option.spreadSizeY = _size * scale;
  // The legacy and PAGX stroke position enums order their values differently, so map by name.
  switch (_position) {
    case StrokePosition::Center:
      option.position = pag::StrokePosition::Center;
      break;
    case StrokePosition::Inside:
      option.position = pag::StrokePosition::Inside;
      break;
    case StrokePosition::Outside:
      option.position = pag::StrokePosition::Outside;
      break;
  }
  // Center/Inside strokes hug the contour band rather than the full silhouette, so the legacy
  // shrinks the dilation accordingly.
  if (_position == StrokePosition::Center) {
    option.spreadSizeX *= 0.4f;
    option.spreadSizeY *= 0.4f;
  } else if (_position == StrokePosition::Inside) {
    option.spreadSizeX *= 0.8f;
    option.spreadSizeY *= 0.8f;
  }
  if (_position == StrokePosition::Outside) {
    return pag::SolidStrokeFilter::CreateFilter(nullptr, option, mode);
  }
  // The Center/Inside shader keeps the original content through its second sampler, and the
  // dilation runs on the detected contour band instead of the full alpha.
  if (original == nullptr) {
    return nullptr;
  }
  auto strokeFilter = pag::SolidStrokeFilter::CreateFilter(nullptr, option, mode, original);
  if (strokeFilter == nullptr) {
    return nullptr;
  }
  return tgfx::ImageFilter::Compose(
      tgfx::ImageFilter::Runtime(std::make_shared<pag::AlphaEdgeDetectLayerEffect>(nullptr)),
      std::move(strokeFilter));
}

std::shared_ptr<tgfx::Image> StrokeLayerFilter::onFilterImage(std::shared_ptr<tgfx::Image> input,
                                                              float scale, const tgfx::Rect&,
                                                              tgfx::Point* offset,
                                                              const tgfx::Point&) {
  if (input == nullptr) {
    return input;
  }
  auto filter = makeStrokeFilter(scale, input);
  if (filter == nullptr) {
    return input;
  }
  tgfx::Point strokeOffset = {};
  auto stroked = input->makeWithFilter(std::move(filter), &strokeOffset);
  if (stroked == nullptr || stroked == input) {
    return input;
  }
  if (offset != nullptr) {
    *offset = strokeOffset;
  }
  if (_alpha >= 1.0f) {
    return stroked;
  }
  // The stroke output replaces the content and is drawn as one image at the style opacity,
  // exactly like the legacy StrokeFilter::draw() paint.
  tgfx::PictureRecorder recorder = {};
  auto* canvas = recorder.beginRecording();
  tgfx::Paint paint = {};
  paint.setAlpha(_alpha);
  paint.setBlendMode(_blendMode);
  canvas->drawImage(stroked, 0.0f, 0.0f, &paint);
  auto picture = recorder.finishRecordingAsPicture();
  if (picture == nullptr) {
    return input;
  }
  auto result = tgfx::Image::MakeFrom(std::move(picture), stroked->width(), stroked->height());
  if (result == nullptr) {
    return input;
  }
  return result;
}

tgfx::Rect StrokeLayerFilter::filterBounds(const tgfx::Rect& srcRect, float contentScale,
                                           tgfx::MapDirection) {
  if (srcRect.isEmpty() || _size <= 0.0f) {
    return srcRect;
  }
  auto factor = 1.0f;
  if (_position == StrokePosition::Center) {
    factor = 0.4f;
  } else if (_position == StrokePosition::Inside) {
    factor = 0.8f;
  }
  // Both directions outset: Forward grows the layer bounds for the stroke ring, Reverse grows
  // the clip so the offscreen input includes the band the dilation samples from.
  auto bounds = srcRect.makeOutset(_size * factor * contentScale, _size * factor * contentScale);
  bounds.roundOut();
  return bounds;
}

}  // namespace pagx
