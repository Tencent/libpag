/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "Graphic.h"
#include "pag/gpu.h"
#include "rendering/graphics/ImageProxy.h"
#include "rendering/graphics/Snapshot.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/gpu/ImageOrigin.h"

namespace pag {
/**
 * Picture describes a two dimensional array of pixels to draw. The pixels may be decoded in a
 * raster bitmap, encoded in compressed data stream, or located in GPU memory as a BackendTexture.
 * Picture cannot be modified after it is created. Picture may allocate additional storage as
 * needed; for instance, an encoded Picture may decode when drawn. Picture width and height are
 * always greater than zero. Creating an Picture with zero width or height returns nullptr.
 */
class Picture : public Graphic {
 public:
  /**
   * Creates a new Picture with specified Image. Return null if the image is null.
   */
  static std::shared_ptr<Graphic> MakeFrom(ID assetID, std::shared_ptr<tgfx::Image> image);

  /*
   * Creates a new image with specified ImageProxy. Returns nullptr if the proxy is null.
   */
  static std::shared_ptr<Graphic> MakeFrom(ID assetID, std::shared_ptr<ImageProxy> proxy);

  /**
   * Creates a new Picture with specified backend texture. Returns null if the texture is invalid.
   */
  static std::shared_ptr<Graphic> MakeFrom(ID assetID, const tgfx::BackendTexture& texture,
                                           tgfx::ImageOrigin origin);

  /**
   * Creates a new Picture with specified graphic. If the assetID is valid (not 0), the returned
   * Picture may be cached as an internal texture representation during rendering, which increases
   * performance for drawing complex content.
   */
  static std::shared_ptr<Graphic> MakeFrom(ID assetID, std::shared_ptr<Graphic> graphic);

  explicit Picture(ID assetID);

  GraphicType type() const final {
    return GraphicType::Picture;
  }

 protected:
  ID assetID = 0;

  virtual float getScaleFactor(float maxScaleFactor) const = 0;
  virtual std::unique_ptr<Snapshot> makeSnapshot(RenderCache* cache, float scaleFactor,
                                                 bool mipmapped) const = 0;

 private:
  uint64_t uniqueKey = 0;

  friend class RenderCache;
};
}  // namespace pag
