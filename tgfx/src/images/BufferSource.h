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

#include "ImageSource.h"

namespace tgfx {
/**
 * BufferSource wraps a decoded ImageBuffer that can generate textures on demand.
 */
class BufferSource : public ImageSource {
 public:
  int width() const override {
    return imageBuffer->width();
  }

  int height() const override {
    return imageBuffer->height();
  }

  bool hasMipmaps() const override {
    return mipMapped;
  }

  bool isAlphaOnly() const override {
    return imageBuffer->isAlphaOnly();
  }

 protected:
  std::shared_ptr<ImageSource> onMakeMipMapped() const override;

  std::shared_ptr<TextureProxy> onMakeTextureProxy(Context* context,
                                                   uint32_t surfaceFlags) const override;

 private:
  std::shared_ptr<ImageBuffer> imageBuffer = nullptr;
  bool mipMapped = false;

  BufferSource(UniqueKey uniqueKey, std::shared_ptr<ImageBuffer> buffer, bool mipMapped = false);

  friend class ImageSource;
  friend class AsyncSource;
};
}  // namespace tgfx
