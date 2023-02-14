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
#include "JNIUtil.h"
#include "tgfx/platform/android/ImageBufferQueue.h"

namespace tgfx {
class SurfaceBufferQueue : public ImageBufferQueue {
 public:
  static void JNIInit(JNIEnv* env);

  SurfaceBufferQueue(JNIEnv* env, jobject surfaceTexture, int width, int height);

  std::shared_ptr<ImageBuffer> acquireNextBuffer() override;

  void notifyFrameAvailable() override;

 private:
  std::mutex locker = {};
  std::condition_variable condition = {};
  std::weak_ptr<SurfaceBufferQueue> weakThis;
  Global<jobject> surfaceTexture;
  int width = 0;
  int height = 0;
  std::shared_ptr<GLExternalTexture> texture = nullptr;
  bool hasNewPendingFrame = false;
  bool frameAvailable = false;

  std::shared_ptr<Texture> makeTexture(Context* context, bool mipMapped = false);
  bool attachToContext(JNIEnv* env, Context* context);

  friend class SurfaceBuffer;
  friend class ImageBufferQueue;
};
}  // namespace tgfx
