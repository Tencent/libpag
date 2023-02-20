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

#include "tgfx/platform/android/SurfaceImageReader.h"

namespace pag {
class JVideoSurface {
 public:
  static std::shared_ptr<tgfx::SurfaceImageReader> GetImageReader(JNIEnv* env,
                                                                  jobject videoSurface);

  explicit JVideoSurface(std::shared_ptr<tgfx::SurfaceImageReader> imageReader)
      : imageReader(imageReader) {
  }

  std::shared_ptr<tgfx::SurfaceImageReader> get() {
    std::lock_guard<std::mutex> autoLock(locker);
    return imageReader;
  }

  void clear() {
    std::lock_guard<std::mutex> autoLock(locker);
    imageReader = nullptr;
  }

 private:
  std::mutex locker;
  std::shared_ptr<tgfx::SurfaceImageReader> imageReader = nullptr;
};
}  // namespace pag
