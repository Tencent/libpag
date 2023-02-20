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
 * The SurfaceImageReader class allows direct access to image data rendered into a Surface on the
 * android platform. SurfaceImageReader is safe across threads.
 */
class SurfaceImageReader : public ImageReader {
 public:
  /**
   * Creates a new SurfaceImageReader with the specified image size and listener. Returns nullptr
   * if the image size is zero or the listener is null.
   * @param width The width of generated ImageBuffers.
   * @param height The height of generated ImageBuffers.
   * @param listener A java object that implements the SurfaceTexture.OnFrameAvailableListener
   * interface. The implementation should make a native call to the notifyFrameAvailable() of the
   * returned SurfaceImageReader.
   */
  static std::shared_ptr<SurfaceImageReader> Make(int width, int height, jobject listener);

  /**
   * Returns the Surface object used as the input to the SurfaceImageReader. The release() method of
   * the returned Surface will be called when the SurfaceImageReader is released.
   */
  virtual jobject getInputSurface() const = 0;

  /**
   * Acquires the next ImageBuffer from the SurfaceImageReader after a new image frame has been
   * rendered into the associated Surface instance. The returned ImageBuffer will be blocked when
   * generating textures until the SurfaceImageReader.notifyFrameAvailable() method is called.
   * Note that the previously returned image buffers will immediately become invalid after the newly
   * created ImageBuffer is drawn.
   */
  virtual std::shared_ptr<ImageBuffer> acquireNextBuffer() = 0;

  /**
   * Notifies the previously returned ImageBuffer is available for generating textures. The method
   * should be called by the listener passed in when creating the reader.
   */
  virtual void notifyFrameAvailable() = 0;

 protected:
  SurfaceImageReader(int width, int height) : ImageReader(width, height) {
  }
};
}  // namespace tgfx
