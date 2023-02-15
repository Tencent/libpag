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

#include <jni.h>
#include "tgfx/core/ImageBuffer.h"

namespace tgfx {
/**
 * The ImageBufferQueue class allows direct access to image buffers rendered into a SurfaceTexture.
 * ImageBufferQueue is safe to use across threads.
 */
class ImageBufferQueue {
 public:
  /**
   * Creates a new ImageBufferQueue from the specified SurfaceTexture object and the buffer size.
   * Note that the surfaceTexture must be in the detached mode. Returns nullptr if the
   * surfaceTexture is nullptr or the buffer size are zero.
   */
  static std::shared_ptr<ImageBufferQueue> MakeFrom(jobject surfaceTexture, int width, int height);

  virtual ~ImageBufferQueue() = default;

  /**
   * Acquires the next ImageBuffer from the ImageBufferQueue after a new image frame has been sent
   * into the associated SurfaceTexture. The returned ImageBuffer is in the pending mode, which will
   * be blocked when generating textures until the ImageBufferQueue.notifyFrameAvailable() method
   * is called. Note that because there can be only one pending ImageBuffer for each
   * ImageBufferQueue at a time, the previously returned image buffers will immediately become
   * invalid after calling this method.
   */
  virtual std::shared_ptr<ImageBuffer> acquireNextBuffer() = 0;

  /**
   * Notifies the previously returned pending ImageBuffer is available for generating textures. You
   * should call the method on a different thread from the ImageBuffer's drawing thread.
   */
  virtual void notifyFrameAvailable() = 0;
};
}  // namespace tgfx
