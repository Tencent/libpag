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
#include "tgfx/core/ImageReader.h"

namespace tgfx {
/**
 * The SurfaceTextureReader class allows direct access to image data rendered into a SurfaceTexture
 * object on the android platform. SurfaceTextureReader is safe across threads.
 */
class SurfaceTextureReader : public ImageReader {
 public:
  /**
   * Creates a new SurfaceTextureReader with the specified image size and listener. Returns nullptr
   * if the image size is zero or the listener is null.
   * @param width The width of generated ImageBuffers.
   * @param height The height of generated ImageBuffers.
   * @param listener A java object that implements the SurfaceTexture.OnFrameAvailableListener
   * interface. The implementation should make a native call to the notifyFrameAvailable() of the
   * returned SurfaceTextureReader.
   */
  static std::shared_ptr<SurfaceTextureReader> Make(int width, int height, jobject listener);

  /**
   * Returns the Surface object used as the input to the SurfaceTextureReader. The release() method
   * of the returned Surface will be called when the SurfaceTextureReader is released.
   */
  jobject getInputSurface() const;

  /**
   * Notifies the previously returned ImageBuffer is available for generating textures. The method
   * should be called by the listener passed in when creating the reader.
   */
  void notifyFrameAvailable();

  /**
   * Acquires the next ImageBuffer from the SurfaceTextureReader after a new image frame has been
   * rendered into the associated input Surface instance. The returned ImageBuffer will be blocked
   * when generating textures until the SurfaceTextureReader.notifyFrameAvailable() method is
   * called. Note that the previously returned image buffers will immediately expire after the newly
   * created ImageBuffer is drawn.
   */
  std::shared_ptr<ImageBuffer> acquireNextBuffer() override;

 private:
  explicit SurfaceTextureReader(std::shared_ptr<ImageStream> stream);
};
}  // namespace tgfx
