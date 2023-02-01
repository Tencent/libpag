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

#include <emscripten/val.h>
#include "tgfx/core/ImageBuffer.h"

namespace tgfx {
class NativeImageBuffer : public ImageBuffer {
 public:
  static std::shared_ptr<NativeImageBuffer> Make(emscripten::val source);

  ~NativeImageBuffer() override;

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
  emscripten::val source = emscripten::val::null();

  explicit NativeImageBuffer(int width, int height, emscripten::val source)
      : _width(width), _height(height), source(source) {
  }
};
}  // namespace tgfx
