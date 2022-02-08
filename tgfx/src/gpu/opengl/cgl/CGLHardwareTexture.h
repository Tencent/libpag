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

#import <AppKit/AppKit.h>
#include "gpu/opengl/GLTexture.h"

namespace pag {
class CGLHardwareTexture : public GLTexture {
 public:
  static std::shared_ptr<CGLHardwareTexture> MakeFrom(Context* context,
                                                      CVPixelBufferRef pixelBuffer);

  explicit CGLHardwareTexture(CVPixelBufferRef pixelBuffer);

  ~CGLHardwareTexture() override;
  Point getTextureCoord(float x, float y) const override;
  size_t memoryUsage() const override;

 protected:
  void computeRecycleKey(BytesKey* recycleKey) const override;
  void onRelease(Context*) override;

 private:
  CVPixelBufferRef pixelBuffer = nullptr;
  CVOpenGLTextureRef texture = nil;

  static void ComputeRecycleKey(BytesKey* recycleKey, CVPixelBufferRef pixelBuffer);
};
}  // namespace pag
