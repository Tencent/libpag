/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "tgfx/gpu/Texture.h"

namespace tgfx {
/**
 * RenderTarget represents a 2D buffer of pixels that can be rendered to.
 */
class RenderTarget : public Resource {
 public:
  /**
   * Returns the display width of the render target.
   */
  int width() const {
    return _width;
  }

  /**
   * Returns the display height of the render target.
   */
  int height() const {
    return _height;
  }

  /**
   * Returns the origin of the render target, either ImageOrigin::TopLeft or
   * ImageOrigin::BottomLeft.
   */
  ImageOrigin origin() const {
    return _origin;
  }

  /**
   * Returns the sample count of the render target.
   */
  int sampleCount() const {
    return _sampleCount;
  }

 protected:
  RenderTarget(int width, int height, ImageOrigin origin, int sampleCount = 1)
      : _width(width), _height(height), _origin(origin), _sampleCount(sampleCount) {
  }

 private:
  int _width = 0;
  int _height = 0;
  ImageOrigin _origin = ImageOrigin::TopLeft;
  int _sampleCount = 1;
};
}  // namespace tgfx
