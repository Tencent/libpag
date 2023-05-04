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

#include "gpu/Texture.h"
#include "tgfx/platform/HardwareBuffer.h"

#if defined(__ANDROID__) || defined(ANDROID)
#include "EGLHardwareTexture.h"
#include "platform/android/AHardwareBufferFunctions.h"
#include "tgfx/platform/HardwareBuffer.h"
#endif

namespace tgfx {
#if defined(__ANDROID__) || defined(ANDROID)

bool HardwareBufferAvailable() {
  static const bool available = AHardwareBufferFunctions::Get()->allocate != nullptr &&
                                AHardwareBufferFunctions::Get()->release != nullptr &&
                                AHardwareBufferFunctions::Get()->lock != nullptr &&
                                AHardwareBufferFunctions::Get()->unlock != nullptr &&
                                AHardwareBufferFunctions::Get()->describe != nullptr &&
                                AHardwareBufferFunctions::Get()->acquire != nullptr &&
                                AHardwareBufferFunctions::Get()->toHardwareBuffer != nullptr &&
                                AHardwareBufferFunctions::Get()->fromHardwareBuffer != nullptr;
  return available;
}

std::shared_ptr<Texture> Texture::MakeFrom(Context* context, HardwareBufferRef hardwareBuffer,
                                           YUVColorSpace) {
  if (!HardwareBufferCheck(hardwareBuffer)) {
    return nullptr;
  }
  return EGLHardwareTexture::MakeFrom(context, hardwareBuffer);
}

#else

bool HardwareBufferAvailable() {
  return false;
}

std::shared_ptr<Texture> Texture::MakeFrom(Context*, HardwareBufferRef, YUVColorSpace) {
  return nullptr;
}

#endif
}  // namespace tgfx