/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <emscripten/val.h>
#include "tgfx/core/ImageBuffer.h"

namespace tgfx {
class WebImageBuffer : public ImageBuffer {
 public:
  /**
   * Creates a new ImageBuffer object from the platform-specific nativeImage in the CPU. The
   * returned ImageBuffer object takes a reference to the nativeImage. Returns nullptr if the
   * nativeImage is nullptr or has a size of zero.
   */
  static std::shared_ptr<WebImageBuffer> MakeFrom(emscripten::val nativeImage);

  /**
   * Creates a new ImageBuffer object from the platform-specific nativeImage in the CPU. The
   * returned ImageBuffer object takes a reference to the nativeImage and will call
   * tgfx.releaseNativeImage() when it goes out of scope. Returns nullptr if the nativeImage is
   * nullptr or has a size of zero.
   */
  static std::shared_ptr<WebImageBuffer> MakeAdopted(emscripten::val nativeImage);

  ~WebImageBuffer() override;

  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  bool isAlphaOnly() const override {
    return false;
  }

 protected:
  std::shared_ptr<Texture> onMakeTexture(Context* context, bool mipMapped) const override;

 private:
  int _width = 0;
  int _height = 0;
  emscripten::val nativeImage = emscripten::val::null();
  bool usePromise = false;
  bool adopted = false;

  WebImageBuffer(int width, int height, emscripten::val nativeImage, bool usePromise);

  emscripten::val getImage() const;

  friend class NativeCodec;
};
}  // namespace tgfx
