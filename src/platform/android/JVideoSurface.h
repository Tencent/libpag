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

#include <mutex>
#include "tgfx/platform/android/SurfaceTexture.h"

namespace pag {
class JVideoSurface {
 public:
  static std::shared_ptr<tgfx::SurfaceTexture> GetImageStream(JNIEnv* env, jobject videoSurface);

  explicit JVideoSurface(std::shared_ptr<tgfx::SurfaceTexture> imageReader)
      : imageReader(imageReader) {
  }

  std::shared_ptr<tgfx::SurfaceTexture> get() {
    std::lock_guard<std::mutex> autoLock(locker);
    return imageReader;
  }

  void clear() {
    std::lock_guard<std::mutex> autoLock(locker);
    imageReader = nullptr;
  }

 private:
  std::mutex locker;
  std::shared_ptr<tgfx::SurfaceTexture> imageReader = nullptr;
};
}  // namespace pag
