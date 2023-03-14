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

#include <CoreVideo/CoreVideo.h>
#include "core/PixelBuffer.h"

namespace tgfx {
class HardwareBuffer : public PixelBuffer {
 public:
  static std::shared_ptr<HardwareBuffer> Make(int width, int height, bool alphaOnly = false);

  static std::shared_ptr<HardwareBuffer> MakeFrom(CVPixelBufferRef pixelBuffer);

  ~HardwareBuffer() override;

  bool isHardwareBacked() const override {
    return true;
  }

  void* lockPixels() override;

  void unlockPixels() override;

 protected:
  std::shared_ptr<Texture> onMakeTexture(Context* context, bool mipMapped) const override;

 private:
  CVPixelBufferRef pixelBuffer = nullptr;

  explicit HardwareBuffer(CVPixelBufferRef pixelBuffer);
};

}  // namespace tgfx
