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

#include <android/hardware_buffer.h>
#include "HardwareBufferInterface.h"
#include "tgfx/core/PixelBuffer.h"

namespace tgfx {
class HardwareBuffer : public PixelBuffer {
 public:
  static std::shared_ptr<PixelBuffer> Make(int width, int height, bool alphaOnly);

  static std::shared_ptr<PixelBuffer> MakeFrom(AHardwareBuffer* hardwareBuffer);

  ~HardwareBuffer() override;

  void* lockPixels() override;

  void unlockPixels() override;

  std::shared_ptr<Texture> makeTexture(Context*) const override;

  explicit HardwareBuffer(AHardwareBuffer* hardwareBuffer);

 private:
  AHardwareBuffer* hardwareBuffer = nullptr;
};
}  // namespace tgfx
