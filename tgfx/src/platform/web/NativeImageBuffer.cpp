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

#include "NativeImageBuffer.h"
#include "gpu/opengl/GLContext.h"
#include "tgfx/gpu/opengl/GLTexture.h"

namespace tgfx {
std::shared_ptr<ImageBuffer> ImageBuffer::MakeFromNativeImage(NativeImageRef nativeImage) {
  if (!nativeImage.as<bool>()) {
    return nullptr;
  }
  auto width = nativeImage.call<int>("width");
  auto height = nativeImage.call<int>("height");
  if (width < 1 || height < 1) {
    return nullptr;
  }
  return std::shared_ptr<NativeImageBuffer>(
      new NativeImageBuffer(width, height, std::move(nativeImage), false));
}

std::shared_ptr<Texture> NativeImageBuffer::onMakeTexture(Context* context, bool mipMapped) const {
  auto texture =
      Texture::MakeRGBA(context, width(), height(), nullptr, 0, ImageOrigin::TopLeft, mipMapped);
  if (texture == nullptr) {
    return nullptr;
  }
  auto& glInfo = std::static_pointer_cast<GLTexture>(texture)->glSampler();
  auto gl = GLFunctions::Get(context);
  gl->bindTexture(glInfo.target, glInfo.id);
  getImage().call<void>("upload", emscripten::val::module_property("GL"));
  return texture;
}

NativeImageBuffer::NativeImageBuffer(int width, int height, emscripten::val nativeImage,
                                     bool usePromise)
    : _width(width), _height(height), nativeImage(nativeImage), usePromise(usePromise) {
}

NativeImageBuffer::~NativeImageBuffer() {
  getImage().call<void>("onDestroy");
}

emscripten::val NativeImageBuffer::getImage() const {
  if (usePromise) {
    return nativeImage.await();
  }
  return nativeImage;
}
}  // namespace tgfx
