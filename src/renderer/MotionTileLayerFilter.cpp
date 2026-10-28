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

#include "MotionTileLayerFilter.h"
#include <utility>
#include "rendering/filters/MotionTileFilter.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/MapDirection.h"
#include "tgfx/core/Point.h"

namespace pagx {

std::shared_ptr<MotionTileLayerFilter> MotionTileLayerFilter::Make(
    Point tileCenter, float tileWidth, float tileHeight, float outputWidth, float outputHeight,
    bool mirrorEdges, float phase, bool horizontalPhaseShift) {
  auto filter = std::shared_ptr<MotionTileLayerFilter>(new MotionTileLayerFilter());
  filter->_tileCenter = tileCenter;
  filter->_tileWidth = tileWidth;
  filter->_tileHeight = tileHeight;
  filter->_outputWidth = outputWidth;
  filter->_outputHeight = outputHeight;
  filter->_mirrorEdges = mirrorEdges;
  filter->_phase = phase;
  filter->_horizontalPhaseShift = horizontalPhaseShift;
  return filter;
}

void MotionTileLayerFilter::setTileCenter(float x, float y) {
  if (_tileCenter.x == x && _tileCenter.y == y) {
    return;
  }
  _tileCenter = {x, y};
  invalidateFilter();
}

void MotionTileLayerFilter::setTileWidth(float value) {
  if (_tileWidth == value) {
    return;
  }
  _tileWidth = value;
  invalidateFilter();
}

void MotionTileLayerFilter::setTileHeight(float value) {
  if (_tileHeight == value) {
    return;
  }
  _tileHeight = value;
  invalidateFilter();
}

void MotionTileLayerFilter::setOutputWidth(float value) {
  if (_outputWidth == value) {
    return;
  }
  _outputWidth = value;
  invalidateFilter();
}

void MotionTileLayerFilter::setOutputHeight(float value) {
  if (_outputHeight == value) {
    return;
  }
  _outputHeight = value;
  invalidateFilter();
}

void MotionTileLayerFilter::setMirrorEdges(bool value) {
  if (_mirrorEdges == value) {
    return;
  }
  _mirrorEdges = value;
  invalidateFilter();
}

void MotionTileLayerFilter::setPhase(float value) {
  if (_phase == value) {
    return;
  }
  _phase = value;
  invalidateFilter();
}

void MotionTileLayerFilter::setHorizontalPhaseShift(bool value) {
  if (_horizontalPhaseShift == value) {
    return;
  }
  _horizontalPhaseShift = value;
  invalidateFilter();
}

std::shared_ptr<tgfx::Image> MotionTileLayerFilter::onFilterImage(
    std::shared_ptr<tgfx::Image> input, float, const tgfx::Rect& contentBounds,
    tgfx::Point* offset) {
  if (input == nullptr || contentBounds.isEmpty()) {
    return input;
  }
  // Mirrors the legacy MotionTileFilter::Apply: the tile center normalizes into uv fractions of
  // the content bounds; the remaining values carry over raw (percent-like shader inputs).
  auto filter = std::make_shared<pag::MotionTileFilter>(
      nullptr,
      pag::Point{_tileCenter.x / contentBounds.width(), _tileCenter.y / contentBounds.height()},
      _tileWidth, _tileHeight, _outputWidth, _outputHeight, _mirrorEdges, _phase,
      _horizontalPhaseShift);
  return input->makeWithFilter(tgfx::ImageFilter::Runtime(std::move(filter)), offset);
}

tgfx::Rect MotionTileLayerFilter::filterBounds(const tgfx::Rect& srcRect, float,
                                               tgfx::MapDirection) {
  // Mirrors the legacy MotionTileEffect::transformBounds: the output region scales the content
  // bounds by the output percent, centered. The mapping applies to both directions: the legacy
  // never needed a reverse mapping (its input texture was fixed at the content bounds), and a
  // tiling filter always needs its full source, which the expansion keeps intact (intersecting
  // the expanded rect preserves the original content rect).
  auto width = srcRect.width() * _outputWidth / 100.0f;
  auto height = srcRect.height() * _outputHeight / 100.0f;
  auto x = srcRect.left + (srcRect.width() - width) * 0.5f;
  auto y = srcRect.top + (srcRect.height() - height) * 0.5f;
  return tgfx::Rect::MakeXYWH(x, y, width, height);
}

}  // namespace pagx
