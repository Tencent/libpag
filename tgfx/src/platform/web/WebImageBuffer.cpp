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

#include "WebImageBuffer.h"
#include "gpu/Texture.h"
#include "opengl/GLSampler.h"
#include "tgfx/core/ImageCodec.h"

using namespace emscripten;

namespace tgfx {
std::shared_ptr<WebImageBuffer> WebImageBuffer::MakeFrom(emscripten::val nativeImage) {
  if (nativeImage.isNull()) {
    return nullptr;
  }
  auto size = val::module_property("tgfx").call<val>("getSourceSize", nativeImage);
  auto width = size["width"].as<int>();
  auto height = size["height"].as<int>();
  if (width < 1 || height < 1) {
    return nullptr;
  }
  return std::shared_ptr<WebImageBuffer>(new WebImageBuffer(width, height, nativeImage, false));
}

std::shared_ptr<WebImageBuffer> WebImageBuffer::MakeAdopted(emscripten::val nativeImage) {
  auto imageBuffer = MakeFrom(nativeImage);
  if (imageBuffer != nullptr) {
    imageBuffer->adopted = true;
  }
  return imageBuffer;
}

WebImageBuffer::WebImageBuffer(int width, int height, emscripten::val nativeImage, bool usePromise)
    : _width(width), _height(height), nativeImage(nativeImage), usePromise(usePromise) {
}

WebImageBuffer::~WebImageBuffer() {
  if (adopted) {
    val::module_property("tgfx").call<void>("releaseNativeImage", nativeImage);
  }
}

std::shared_ptr<Texture> WebImageBuffer::onMakeTexture(Context* context, bool) const {
  auto texture = Texture::MakeRGBA(context, width(), height(), nullptr, 0, ImageOrigin::TopLeft);
  if (texture == nullptr) {
    return nullptr;
  }
  auto glInfo = static_cast<const GLSampler*>(texture->getSampler());
  val::module_property("tgfx").call<void>("uploadToTexture", emscripten::val::module_property("GL"),
                                          getImage(), glInfo->id, false);
  return texture;
}

emscripten::val WebImageBuffer::getImage() const {
  if (usePromise) {
    return nativeImage.await();
  }
  return nativeImage;
}
}  // namespace tgfx
