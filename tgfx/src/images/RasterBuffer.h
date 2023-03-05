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

#include "tgfx/core/Data.h"
#include "tgfx/core/ImageBuffer.h"
#include "tgfx/core/ImageInfo.h"

namespace tgfx {
class RasterBuffer : public ImageBuffer {
 public:
  static std::shared_ptr<ImageBuffer> MakeFrom(const ImageInfo& info, std::shared_ptr<Data> pixels);

  int width() const override {
    return info.width();
  }

  int height() const override {
    return info.height();
  }

  bool isAlphaOnly() const override {
    return info.isAlphaOnly();
  }

 protected:
  std::shared_ptr<Texture> onMakeTexture(Context* context, bool mipMapped) const override;

 private:
  ImageInfo info = {};
  std::shared_ptr<Data> pixels = nullptr;

  RasterBuffer(const ImageInfo& info, std::shared_ptr<Data> pixels);
};
}  // namespace tgfx
