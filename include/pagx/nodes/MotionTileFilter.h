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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "pagx/nodes/LayerFilter.h"
#include "pagx/types/Point.h"

namespace pagx {

/**
 * A motion tile filter (spec 7.5): tiles the layer content around a tile center with optional
 * edge mirroring and phase-driven sliding, migrated from the legacy MotionTile effect.
 */
class MotionTileFilter : public LayerFilter {
 public:
  /**
   * The center of the tiling in layer coordinates. The default value is (0, 0).
   */
  Point tileCenter = {};

  /**
   * The tile width in pixels. The default value is 0.
   */
  float tileWidth = 0.0f;

  /**
   * The tile height in pixels. The default value is 0.
   */
  float tileHeight = 0.0f;

  /**
   * The output width in pixels. The default value is 0.
   */
  float outputWidth = 0.0f;

  /**
   * The output height in pixels. The default value is 0.
   */
  float outputHeight = 0.0f;

  /**
   * Whether tile edges repeat mirrored. The default value is false.
   */
  bool mirrorEdges = false;

  /**
   * The sliding phase of the tiles. The default value is 0.
   */
  float phase = 0.0f;

  /**
   * Whether the phase shift applies horizontally instead of vertically. The default is false.
   */
  bool horizontalPhaseShift = false;

  NodeType nodeType() const override {
    return NodeType::MotionTileFilter;
  }

 private:
  MotionTileFilter() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
