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
class ProxyProvider;

/**
 * This class delays the acquisition of textures until they are actually required.
 */
class TextureProxy {
 public:
  virtual ~TextureProxy();

  /**
   * Returns the width of the texture proxy.
   */
  int width() const {
    return _width;
  }

  /**
   * Returns the height of the texture proxy.
   */
  int height() const {
    return _height;
  }

  /**
   * Creates a new Texture associated with specified context. Returns nullptr if failed.
   */
  std::shared_ptr<Texture> getTexture() const {
    return texture;
  }

  /**
   * Returns true if the backing texture is instantiated.
   */
  bool isInstantiated() const {
    return texture != nullptr;
  }

  /**
   * Actually instantiate the backing texture, if necessary. Returns true if the backing texture is
   * instantiated.
   */
  bool instantiate();

 protected:
  std::shared_ptr<Cacheable> contentOwner = nullptr;

  TextureProxy(ProxyProvider* provider, int width, int height);

  /**
   * Overrides to create a new Texture associated with specified context.
   */
  virtual std::shared_ptr<Texture> onMakeTexture(Context* context);

 private:
  std::shared_ptr<Texture> texture = nullptr;
  ProxyProvider* provider = nullptr;
  int _width = 0;
  int _height = 0;

  friend class ProxyProvider;
};
}  // namespace tgfx
