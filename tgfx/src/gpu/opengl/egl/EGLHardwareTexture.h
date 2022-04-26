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

#if defined(__ANDROID__) || defined(ANDROID)

#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "platform/android/HardwareBufferInterface.h"
#include "tgfx/gpu/opengl/GLTexture.h"

namespace tgfx {
class EGLHardwareTexture : public GLTexture {
 public:
  static std::shared_ptr<EGLHardwareTexture> MakeFrom(Context* context,
                                                      AHardwareBuffer* hardwareBuffer);

 private:
  AHardwareBuffer* hardwareBuffer = nullptr;
  EGLImageKHR eglImage = EGL_NO_IMAGE_KHR;

  static void ComputeRecycleKey(BytesKey* recycleKey, void* hardwareBuffer);

  EGLHardwareTexture(AHardwareBuffer* hardwareBuffer, EGLImageKHR eglImage, int width, int height);

  ~EGLHardwareTexture() override;

  void onReleaseGPU() override;
};
}  // namespace tgfx

#endif