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
#include "tgfx/platform/ImageReader.h"

namespace tgfx {
/**
 * The SurfaceTextureReader class allows direct access to image buffers rendered into a
 * SurfaceTexture on the android platform. SurfaceTextureReader is safe across threads.
 */
class SurfaceTextureReader : public ImageReader {
 public:
  /**
   * Creates a new SurfaceTextureReader width the specified SurfaceTexture object and the image
   * size. Note that the surfaceTexture must be in the detached mode. Returns nullptr if the
   * surfaceTexture is nullptr or the image size are zero.
   */
  static std::shared_ptr<SurfaceTextureReader> MakeFrom(jobject surfaceTexture, int width,
                                                        int height);

  /**
   * Acquires the next ImageBuffer from the SurfaceTextureReader after a new image frame has been
   * sent into the associated SurfaceTexture. The returned ImageBuffer will be blocked when
   * generating textures until the SurfaceTextureReader.notifyFrameAvailable() method is called.
   * Note that the previously returned image buffers will immediately become invalid after the newly
   * created ImageBuffer is drawn.
   */
  virtual std::shared_ptr<ImageBuffer> acquireNextBuffer() = 0;

  /**
   * Notifies the previously returned ImageBuffer is available for generating textures. The method
   * should be called on a different thread from the ImageBuffer's drawing thread.
   */
  virtual void notifyFrameAvailable() = 0;

 protected:
  SurfaceTextureReader(int width, int height) : ImageReader(width, height) {}
};
}  // namespace tgfx
