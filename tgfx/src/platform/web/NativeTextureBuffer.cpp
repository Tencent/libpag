/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "NativeTextureBuffer.h"
#include "gpu/opengl/GLContext.h"
#include "tgfx/gpu/opengl/GLTexture.h"

using namespace emscripten;

namespace tgfx {
std::shared_ptr<NativeTextureBuffer> NativeTextureBuffer::Make(int width, int height, val source) {
  if (!source.as<bool>()) {
    return nullptr;
  }
  return std::shared_ptr<NativeTextureBuffer>(
      new NativeTextureBuffer(width, height, std::move(source)));
}

std::shared_ptr<Texture> NativeTextureBuffer::makeTexture(Context* context) const {
  auto texture = Texture::MakeRGBA(context, width(), height(), nullptr, 0);
  if (texture == nullptr) {
    return nullptr;
  }
  auto& glInfo = std::static_pointer_cast<GLTexture>(texture)->glSampler();
  auto gl = GLFunctions::Get(context);
  gl->bindTexture(glInfo.target, glInfo.id);
  source.call<void>("upload", val::module_property("GL"));
  return texture;
}
}  // namespace tgfx
