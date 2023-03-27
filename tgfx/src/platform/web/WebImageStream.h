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
#include "core/ImageStream.h"

namespace tgfx {
/**
 * The WebImageStream class allows direct access to image buffers rendered into a TexImageSource
 * object on the web platform. It is typically used with the ImageReader class.
 */
class WebImageStream : public ImageStream {
 public:
  /**
   * Creates a new WebImageStream from the specified TexImageSource object and the size. Returns
   * nullptr if the source is null or the buffer size is zero.
   */
  static std::shared_ptr<WebImageStream> MakeFrom(emscripten::val source, int width, int height,
                                                  bool alphaOnly = false);

  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  bool isAlphaOnly() const override {
    return alphaOnly;
  }

  bool isHardwareBacked() const override {
    return false;
  }

  bool expired() const override {
    return contentVersion < imageReader->textureVersion;
  }

 protected:
  WebImageStream(emscripten::val source, int width, int height, bool alphaOnly);

  std::shared_ptr<Texture> onMakeTexture(Context* context, bool mipMapped) override;

  bool onUpdateTexture(std::shared_ptr<Texture> texture, const Rect& bounds) override;

 private:
  emscripten::val source = emscripten::val::null();
  int _width = 0;
  int _height = 0;
  bool alphaOnly = false;

  friend class WebMask;
};
}  // namespace tgfx
