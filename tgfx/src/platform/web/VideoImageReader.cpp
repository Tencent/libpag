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

#include "tgfx/platform/web/VideoImageReader.h"
#include "GLVideoTexture.h"
#include "opengl/GLSampler.h"
#include "tgfx/platform/web/WebImage.h"
#include "utils/Log.h"

namespace tgfx {
using namespace emscripten;

std::shared_ptr<VideoImageReader> VideoImageReader::MakeFrom(val video, int width, int height) {
  if (video == val::null() || width < 1 || height < 1) {
    return nullptr;
  }
  auto reader = std::shared_ptr<VideoImageReader>(new VideoImageReader(video, width, height));
  reader->weakThis = reader;
  return reader;
}

VideoImageReader::VideoImageReader(emscripten::val video, int width, int height)
    : ImageReader(width, height), video(video) {
}

std::shared_ptr<ImageBuffer> VideoImageReader::acquireNextBuffer(val promise) {
  if (promise == val::null()) {
    return nullptr;
  }
  if (!WebImage::AsyncSupport()) {
    promise.await();
  }
  currentPromise = promise;
  return makeNextBuffer();
}

bool VideoImageReader::onUpdateTexture(Context* context, bool) {
  if (texture != nullptr && texture->getContext() != context) {
    LOGE(
        "VideoImageReader::onUpdateTexture(): NativeImageReader has already attached to a "
        "Context!");
    return false;
  }
  if (currentPromise == val::null()) {
    return false;
  }
  currentPromise.await();
  if (texture == nullptr) {
    texture = GLVideoTexture::Make(context, width(), height());
  }
  if (texture == nullptr) {
    return false;
  }
  auto glInfo = static_cast<const GLSampler*>(texture->getSampler());
  val::module_property("tgfx").call<void>("uploadToTexture", emscripten::val::module_property("GL"),
                                          video, glInfo->id);
  currentPromise = val::null();
  return true;
}

}  // namespace tgfx