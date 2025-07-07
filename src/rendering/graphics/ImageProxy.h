/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "tgfx/core/Image.h"

namespace pag {
class RenderCache;

/**
 * This class delays the acquisition of images until they are actually required.
 */
class ImageProxy {
 public:
  virtual ~ImageProxy() = default;

  /**
   * Returns the width of the target image.
   */
  virtual int width() const = 0;

  /**
   * Returns the height of the target image.
   */
  virtual int height() const = 0;

  /**
   * Return true if the ImageProxy was created for temporary use, which means it prefers being
   * directly drawn over cached.
   */
  virtual bool isTemporary() const = 0;

  /**
   * Prepares the image for the next getImage() call.
   */
  virtual void prepareImage(RenderCache* cache) const = 0;

  /**
   * Returns an image of the proxy.
   */
  virtual std::shared_ptr<tgfx::Image> getImage(RenderCache* cache) const = 0;

 protected:
  virtual std::shared_ptr<tgfx::Image> makeImage(RenderCache* cache) const = 0;

  friend class RenderCache;
};
}  // namespace pag
