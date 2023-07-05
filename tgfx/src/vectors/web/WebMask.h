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
#include "platform/web/WebImageStream.h"
#include "tgfx/core/Mask.h"

namespace tgfx {
class WebMask : public Mask {
 public:
  WebMask(std::shared_ptr<ImageBuffer> buffer, std::shared_ptr<WebImageStream> stream,
          emscripten::val webMask);

  int width() const override {
    return stream->width();
  }

  int height() const override {
    return stream->height();
  }

  bool isHardwareBacked() const override {
    return stream->width();
  }

  void clear() override;

  std::shared_ptr<ImageBuffer> makeBuffer() const override {
    return buffer;
  }

 protected:
  std::shared_ptr<ImageStream> getImageStream() const override {
    return stream;
  }

  void onFillPath(const Path& path, const Matrix& matrix, bool) override;

  bool onFillText(const TextBlob* textBlob, const Stroke* stroke, const Matrix& matrix) override;

 private:
  std::shared_ptr<ImageBuffer> buffer = nullptr;
  std::shared_ptr<WebImageStream> stream = nullptr;
  emscripten::val webMask = emscripten::val::null();

  void aboutToFill();
};
}  // namespace tgfx
