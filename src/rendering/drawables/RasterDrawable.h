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

#include "OffscreenDrawable.h"

namespace pag {
class RasterDrawable : public OffscreenDrawable {
 public:
  static std::shared_ptr<RasterDrawable> Make(int width, int height);

  void setHardwareBuffer(tgfx::HardwareBufferRef buffer);

  void setPixelBuffer(const tgfx::ImageInfo& info, void* pixels);

  bool isPixelCopied() const {
    return pixelCopied;
  }

 protected:
  std::shared_ptr<tgfx::Surface> onCreateSurface(tgfx::Context* context) override;

 private:
  std::shared_ptr<tgfx::Surface> offscreenSurface = nullptr;
  tgfx::HardwareBufferRef hardwareBuffer = nullptr;
  tgfx::ImageInfo info = {};
  void* pixels = nullptr;
  bool pixelCopied = false;

  RasterDrawable(int width, int height, std::shared_ptr<tgfx::Device> device);

  void present(tgfx::Context* context) override;
};
}  // namespace pag
