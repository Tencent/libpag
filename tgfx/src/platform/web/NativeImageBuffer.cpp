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
#include "gpu/opengl/GLSampler.h"
#include "tgfx/core/ImageCodec.h"

using namespace emscripten;

namespace tgfx {
std::shared_ptr<ImageBuffer> NativeImageBuffer::MakeFrom(emscripten::val nativeImage,
                                                         ReleaseProc releaseProc) {
  auto codec = ImageCodec::MakeFrom(nativeImage);
  if (codec == nullptr) {
    return nullptr;
  }
  auto nativeBuffer = std::static_pointer_cast<NativeImageBuffer>(codec->makeBuffer());
  nativeBuffer->releaseProc = releaseProc;
  return nativeBuffer;
}

std::shared_ptr<Texture> NativeImageBuffer::onMakeTexture(Context* context, bool) const {
  auto texture = Texture::MakeRGBA(context, width(), height(), nullptr, 0, SurfaceOrigin::TopLeft);
  if (texture == nullptr) {
    return nullptr;
  }
  auto glInfo = static_cast<const GLSampler*>(texture->getSampler());
  val::module_property("tgfx").call<void>("uploadToTexture", emscripten::val::module_property("GL"),
                                          getImage(), glInfo->id);
  return texture;
}

NativeImageBuffer::NativeImageBuffer(int width, int height, emscripten::val nativeImage,
                                     bool usePromise)
    : _width(width), _height(height), nativeImage(nativeImage), usePromise(usePromise) {
}

NativeImageBuffer::~NativeImageBuffer() {
  if (releaseProc != nullptr) {
    releaseProc(nativeImage);
  }
}

emscripten::val NativeImageBuffer::getImage() const {
  if (usePromise) {
    return nativeImage.await();
  }
  return nativeImage;
}
}  // namespace tgfx
