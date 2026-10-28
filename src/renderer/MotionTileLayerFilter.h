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
#include "pagx/types/Point.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/Rect.h"
#include "tgfx/layers/filters/LayerFilter.h"

namespace tgfx {
struct Point;
}

namespace pagx {

// Layer filter for the PAGX MotionTileFilter node (spec 7.5). Reuses the legacy motion tile
// RuntimeFilter (src/rendering/filters/MotionTileFilter.cpp) so both pipelines tile identically;
// it runs cache-less because the PAGX layer path has no pag::RenderCache. The tile/output sizes
// and phase carry over raw (the legacy stores percent-like values the shader consumes as-is).
// The bounds replicate the legacy transformBounds: the output region scales the content bounds
// by outputWidth/outputHeight percent, centered.
class MotionTileLayerFilter : public tgfx::LayerFilter {
 public:
  static std::shared_ptr<MotionTileLayerFilter> Make(Point tileCenter, float tileWidth,
                                                     float tileHeight, float outputWidth,
                                                     float outputHeight, bool mirrorEdges,
                                                     float phase, bool horizontalPhaseShift);

  float tileCenterX() const {
    return _tileCenter.x;
  }

  float tileCenterY() const {
    return _tileCenter.y;
  }

  void setTileCenter(float x, float y);

  float tileWidth() const {
    return _tileWidth;
  }

  float tileHeight() const {
    return _tileHeight;
  }

  float outputWidth() const {
    return _outputWidth;
  }

  float outputHeight() const {
    return _outputHeight;
  }

  float phase() const {
    return _phase;
  }

  bool mirrorEdges() const {
    return _mirrorEdges;
  }

  bool horizontalPhaseShift() const {
    return _horizontalPhaseShift;
  }

  void setTileWidth(float value);
  void setTileHeight(float value);
  void setOutputWidth(float value);
  void setOutputHeight(float value);
  void setMirrorEdges(bool value);
  void setPhase(float value);
  void setHorizontalPhaseShift(bool value);

 protected:
  std::shared_ptr<tgfx::Image> onFilterImage(std::shared_ptr<tgfx::Image> input, float scale,
                                             const tgfx::Rect& contentBounds,
                                             tgfx::Point* offset) override;

  tgfx::Rect filterBounds(const tgfx::Rect& srcRect, float contentScale,
                          tgfx::MapDirection direction) override;

 private:
  Point _tileCenter = {};
  float _tileWidth = 0.0f;
  float _tileHeight = 0.0f;
  float _outputWidth = 0.0f;
  float _outputHeight = 0.0f;
  bool _mirrorEdges = false;
  float _phase = 0.0f;
  bool _horizontalPhaseShift = false;
};

}  // namespace pagx
