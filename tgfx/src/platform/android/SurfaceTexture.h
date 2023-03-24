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

#include "core/ImageStream.h"
#include "tgfx/platform/android/Global.h"

namespace tgfx {
/**
 * The SurfaceTexture class allows direct access to image data rendered into the Java Surface object
 * on the android platform. It is typically used with the ImageReader class.
 */
class SurfaceTexture : public ImageStream {
 public:
  /**
   * Creates a new SurfaceTexture with the specified image size and listener. Returns nullptr
   * if the image size is zero or the listener is null.
   * @param width The width of generated ImageBuffers.
   * @param height The height of generated ImageBuffers.
   * @param listener A java object that implements the SurfaceTexture.OnFrameAvailableListener
   * interface. The implementation should make a native call to the notifyFrameAvailable() of the
   * returned SurfaceImageReader.
   */
  static std::shared_ptr<SurfaceTexture> Make(int width, int height, jobject listener);

  ~SurfaceTexture() override;

  int width() const override {
    return _width;
  }

  int height() const override {
    return _height;
  }

  bool isHardwareBacked() const override {
    return false;
  }

  /**
   * Returns the Surface object used as the input to the SurfaceTexture. The release() method of
   * the returned Surface will be called when the SurfaceTexture is released.
   */
  jobject getInputSurface() const;

  /**
   * Notifies the SurfaceTexture that a new image frame has been rendered into the associated
   * input Surface.
   */
  void notifyFrameChanged();

  /**
   * Notifies the previously returned ImageBuffer is available for generating textures. The method
   * should only be called by the listener passed in when creating the reader.
   */
  void notifyFrameAvailable();

 protected:
  std::shared_ptr<Texture> onMakeTexture(Context* context, bool mipMapped) override;

  bool onUpdateTexture(std::shared_ptr<Texture> texture, const Rect& bounds) override;

 private:
  std::mutex locker = {};
  int _width = 0;
  int _height = 0;
  std::condition_variable condition = {};
  Global<jobject> surface;
  Global<jobject> surfaceTexture;
  bool frameAvailable = false;

  static void JNIInit(JNIEnv* env);

  SurfaceTexture(int width, int height, JNIEnv* env, jobject surfaceTexture);

  std::shared_ptr<Texture> makeTexture(Context* context);

  friend class JNIInit;
};
}  // namespace tgfx
