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

#include "EncodedSource.h"

namespace tgfx {
/**
 * AsyncSource wraps an existing EncodedSource and schedules an asynchronous decoding task
 * immediately. AsyncSource uses the original EncodedSource as the cache owner for all generated
 * proxies and textures.
 */
class AsyncSource : public ImageSource {
 public:
  int width() const override {
    return encodedSource->width();
  }

  int height() const override {
    return encodedSource->height();
  }

  bool hasMipmaps() const override {
    return encodedSource->hasMipmaps();
  }

  bool isAlphaOnly() const override {
    return encodedSource->isAlphaOnly();
  }

 protected:
  const Cacheable* getCacheOwner() const override;

  std::shared_ptr<ImageSource> onMakeMipMapped() const override;

  std::shared_ptr<TextureProxy> onMakeTextureProxy(Context* context,
                                                   uint32_t surfaceFlags) const override;

 private:
  std::shared_ptr<ImageGeneratorTask> imageTask = nullptr;
  std::shared_ptr<ImageBuffer> imageBuffer = nullptr;
  std::shared_ptr<EncodedSource> encodedSource = nullptr;

  explicit AsyncSource(std::shared_ptr<EncodedSource> source);

  friend class EncodedSource;
};
}  // namespace tgfx
