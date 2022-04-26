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

#import <CoreVideo/CoreVideo.h>
#include "tgfx/gpu/opengl/GLYUVTexture.h"

namespace tgfx {
class EAGLNV12Texture : public GLYUVTexture {
 public:
  static std::shared_ptr<EAGLNV12Texture> MakeFrom(Context* context, CVPixelBufferRef pixelBuffer,
                                                   YUVColorSpace colorSpace,
                                                   YUVColorRange colorRange);

  EAGLNV12Texture(CVPixelBufferRef pixelBuffer, YUVColorSpace colorSpace, YUVColorRange colorRange);

  ~EAGLNV12Texture() override;

  YUVPixelFormat pixelFormat() const override {
    return YUVPixelFormat::NV12;
  }

 private:
  CVPixelBufferRef pixelBuffer = nullptr;
  CVOpenGLESTextureRef lumaTexture = nullptr;
  CVOpenGLESTextureRef chromaTexture = nullptr;

  void onReleaseGPU() override;
};
}  // namespace tgfx
