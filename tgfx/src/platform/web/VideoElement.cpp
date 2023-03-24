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

#include "tgfx/platform/web/VideoElement.h"
#include "GLVideoTexture.h"
#include "gpu/Gpu.h"
#include "opengl/GLSampler.h"
#include "tgfx/platform/web/WebImage.h"
#include "utils/Log.h"

namespace tgfx {
using namespace emscripten;

std::shared_ptr<VideoElement> VideoElement::MakeFrom(val video, int width, int height) {
  if (video == val::null() || width < 1 || height < 1) {
    return nullptr;
  }
  return std::shared_ptr<VideoElement>(new VideoElement(video, width, height));
}

VideoElement::VideoElement(emscripten::val video, int width, int height)
    : video(video), _width(width), _height(height) {
}

void VideoElement::notifyFrameChanged(emscripten::val promise) {
  if (promise == val::null()) {
    return;
  }
  if (!WebImage::AsyncSupport()) {
    promise.await();
  }
  currentPromise = promise;
  markContentDirty(Rect::MakeWH(_width, _height));
}

std::shared_ptr<Texture> VideoElement::onMakeTexture(Context* context, bool mipMapped) {
  auto texture = GLVideoTexture::Make(context, width(), height(), mipMapped);
  if (texture != nullptr) {
    onUpdateTexture(texture, Rect::MakeWH(_width, _height));
  }
  return texture;
}

bool VideoElement::onUpdateTexture(std::shared_ptr<Texture> texture, const Rect&) {
  if (currentPromise == val::null()) {
    return false;
  }
  if (WebImage::AsyncSupport()) {
    currentPromise.await();
  }
  auto glSampler = static_cast<const GLSampler*>(texture->getSampler());
  val::module_property("tgfx").call<void>("uploadToTexture", emscripten::val::module_property("GL"),
                                          video, glSampler->id);
  if (glSampler->hasMipmaps()) {
    auto gpu = texture->getContext()->gpu();
    gpu->regenerateMipMapLevels(glSampler);
  }
  return true;
}

}  // namespace tgfx