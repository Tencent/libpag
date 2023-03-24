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

#include <emscripten/val.h>
#include "core/ImageStream.h"

namespace tgfx {
/**
 * The VideoElement class allows direct access to image buffers rendered into a HTMLVideoElement
 * on the web platform. It is typically used with the ImageReader class.
 */
class VideoElement : public ImageStream {
 public:
  /**
   * Creates a new VideoElement from the specified HTMLVideoElement object and the video size.
   * Returns nullptr if the video is null or the buffer size is zero.
   */
  static std::shared_ptr<VideoElement> MakeFrom(emscripten::val video, int width, int height);

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
   * Notifies the VideoElement that a new image frame has been rendered into the associated
   * HTMLVideoElement. The next acquired ImageBuffer will call the promise.await() method before
   * generating textures.
   */
  void notifyFrameChanged(emscripten::val promise = emscripten::val::null());

 protected:
  std::shared_ptr<Texture> onMakeTexture(Context* context, bool mipMapped) override;

  bool onUpdateTexture(std::shared_ptr<Texture> texture, const Rect& bounds) override;

 private:
  emscripten::val video = emscripten::val::null();
  emscripten::val currentPromise = emscripten::val::null();
  int _width = 0;
  int _height = 0;

  VideoElement(emscripten::val video, int width, int height);
};
}  // namespace tgfx
