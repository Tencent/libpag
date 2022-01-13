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

#include "GLHardwareTexture.h"
#include "image/PixelBuffer.h"

namespace pag {
class NativeHardwareBuffer : public PixelBuffer {
 public:
  static std::shared_ptr<NativeHardwareBuffer> MakeAdopted(CVPixelBufferRef pixelBuffer);

  ~NativeHardwareBuffer() override;

  void* lockPixels() override;

  void unlockPixels() override;

  std::shared_ptr<Texture> makeTexture(Context* context) const override {
    return GLHardwareTexture::MakeFrom(context, pixelBuffer, adopted);
  }

 private:
  CVPixelBufferRef pixelBuffer = nullptr;
  bool adopted = false;

  NativeHardwareBuffer(CVPixelBufferRef pixelBuffer, bool adopted);

  friend class CocoaPlatform;
};

}  // namespace pag
