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

namespace pag {
class RenderCache;

/**
 * This class delays the acquisition of textures until they are actually required.
 */
class TextureProxy {
 public:
  TextureProxy(int width, int height) : _width(width), _height(height) {
  }

  virtual ~TextureProxy() = default;

  /**
   * Returns the width of the texture.
   */
  int width() const {
    return _width;
  }

  /**
   * Returns the height of the texture.
   */
  int height() const {
    return _height;
  }

  /**
   * If true, this proxy returns the same texture every time. Otherwise, it creates a new texture
   * every time.
   */
  virtual bool cacheEnabled() const = 0;

  /**
   * Prepares this TextureProxy for next getTexture() call.
   */
  virtual void prepare(RenderCache* cache) const = 0;

  /**
   * Instantiates and returns the backing texture associated with this proxy.
   */
  virtual std::shared_ptr<tgfx::Texture> getTexture(RenderCache* cache) const = 0;

 private:
  int _width = 0;
  int _height = 0;
};
}  // namespace pag
