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
 * EncodedSource wraps an ImageGenerator that can generate ImageBuffers on demand.
 */
class EncodedSource : public ImageSource {
 public:
  int width() const override {
    return generator->width();
  }

  int height() const override {
    return generator->height();
  }

  bool hasMipmaps() const override {
    return mipMapped;
  }

  bool isAlphaOnly() const override {
    return generator->isAlphaOnly();
  }

  bool isLazyGenerated() const override {
    return true;
  }

 protected:
  std::shared_ptr<ImageSource> onMakeDecoded(Context* context) const override;

  std::shared_ptr<ImageSource> onMakeMipMapped() const override;

  std::shared_ptr<TextureProxy> onMakeTextureProxy(Context* context,
                                                   uint32_t surfaceFlags) const override;

 private:
  std::shared_ptr<ImageGenerator> generator = nullptr;
  bool mipMapped = false;

  EncodedSource(std::shared_ptr<ImageGenerator> generator, bool mipMapped);

  friend class AsyncSource;
  friend class ImageSource;
};
}  // namespace tgfx
