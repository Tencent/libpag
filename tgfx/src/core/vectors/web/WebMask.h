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
#include "tgfx/core/Mask.h"

namespace tgfx {
class WebMask : public Mask {
 public:
  explicit WebMask(int width, int height, emscripten::val webMask)
      : Mask(width, height), webMask(webMask) {
  }

  void fillPath(const Path& path) override;

  bool fillText(const TextBlob* textBlob) override;

  bool strokeText(const TextBlob* textBlob, const Stroke& stroke) override;

  void clear() override;

  std::shared_ptr<Texture> makeTexture(Context* context) const override;

 private:
  bool drawText(const TextBlob* textBlob, const Stroke* stroke = nullptr);

  emscripten::val webMask = emscripten::val::null();
};
}  // namespace tgfx
