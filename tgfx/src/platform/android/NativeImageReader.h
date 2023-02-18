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

#include <condition_variable>
#include <mutex>
#include "GLExternalTexture.h"
#include "HandlerThread.h"
#include "JNIUtil.h"
#include "tgfx/platform/android/SurfaceImageReader.h"

namespace tgfx {
class NativeImageReader : public SurfaceImageReader {
 public:
  static void JNIInit(JNIEnv* env);

  NativeImageReader(int width, int height, JNIEnv* env, jobject surfaceTexture);

  ~NativeImageReader() override;

  jobject getInputSurface() const override;

  std::shared_ptr<ImageBuffer> acquireNextBuffer() override;

  void notifyFrameAvailable() override;

 protected:
  bool onUpdateTexture(Context* context, bool mipMapped) override;

 private:
  std::mutex locker = {};
  std::condition_variable condition = {};
  Global<jobject> surface;
  Global<jobject> surfaceTexture;
  bool frameAvailable = false;

  bool attachToContext(JNIEnv* env, Context* context);

  friend class SurfaceImageReader;
};
}  // namespace tgfx
