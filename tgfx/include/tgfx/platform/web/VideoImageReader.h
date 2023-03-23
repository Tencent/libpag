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
#include "tgfx/platform/ImageReader.h"

namespace tgfx {
/**
 * The VideoImageReader class allows direct access to image buffers rendered into a HTMLVideoElement
 * on the web platform.
 */
class VideoImageReader : public ImageReader {
 public:
  /**
   * Creates a new BufferQueue from the specified HTMLVideoElement object and the video size.
   * Returns nullptr if the video is null or the buffer size is zero.
   */
  static std::shared_ptr<VideoImageReader> MakeFrom(emscripten::val video, int width, int height);

  /**
   * Acquires the next ImageBuffer from the VideoImageReader after a new image frame has been
   * rendered into the associated HTMLVideoElement. The returned ImageBuffer will call the
   * promise.await() method before generating textures. Note that the previously returned image
   * buffers will immediately expire after the newly created ImageBuffer is drawn.
   */
  std::shared_ptr<ImageBuffer> acquireNextBuffer(emscripten::val promise);

 protected:
  bool onUpdateTexture(Context* context, bool mipMapped) override;

 protected:
  emscripten::val video = emscripten::val::null();
  emscripten::val currentPromise = emscripten::val::null();

  VideoImageReader(emscripten::val video, int width, int height);
};
}  // namespace tgfx
