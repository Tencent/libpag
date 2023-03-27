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

#include "platform/web/VideoElement.h"
#include "GLVideoTexture.h"
#include "tgfx/platform/web/WebCodec.h"

namespace tgfx {
using namespace emscripten;

std::shared_ptr<VideoElement> VideoElement::MakeFrom(val video, int width, int height) {
  if (video == val::null() || width < 1 || height < 1) {
    return nullptr;
  }
  return std::shared_ptr<VideoElement>(new VideoElement(video, width, height));
}

VideoElement::VideoElement(emscripten::val video, int width, int height)
    : WebImageStream(video, width, height, false) {
}

void VideoElement::markFrameChanged(emscripten::val promise) {
  currentPromise = promise;
  markContentDirty(Rect::MakeWH(width(), height()));
  if (currentPromise != val::null() && !WebCodec::AsyncSupport()) {
    currentPromise.await();
  }
}

std::shared_ptr<Texture> VideoElement::onMakeTexture(Context* context, bool mipMapped) {
  auto texture = GLVideoTexture::Make(context, width(), height(), mipMapped);
  if (texture != nullptr) {
    onUpdateTexture(texture, Rect::MakeWH(width(), height()));
  }
  return texture;
}

bool VideoElement::onUpdateTexture(std::shared_ptr<Texture> texture, const Rect& bounds) {
  if (currentPromise != val::null() && WebCodec::AsyncSupport()) {
    currentPromise.await();
  }
  return WebImageStream::onUpdateTexture(texture, bounds);
}

}  // namespace tgfx