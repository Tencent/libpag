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

#include "GLExternalTexture.h"
#include "tgfx/gpu/opengl/GLFunctions.h"

namespace tgfx {
std::shared_ptr<GLExternalTexture> GLExternalTexture::Make(Context* context, int width,
                                                           int height) {
  if (context == nullptr || width < 1 || height < 1) {
    return nullptr;
  }
  auto gl = tgfx::GLFunctions::Get(context);
  tgfx::GLSampler sampler = {};
  sampler.target = GL_TEXTURE_EXTERNAL_OES;
  sampler.format = tgfx::PixelFormat::RGBA_8888;
  gl->genTextures(1, &sampler.id);
  if (sampler.id == 0) {
    return nullptr;
  }
  return Resource::Wrap(context, new GLExternalTexture(std::move(sampler), width, height));
}

GLExternalTexture::GLExternalTexture(GLSampler glSampler, int width, int height)
    : GLTexture(width, height, SurfaceOrigin::TopLeft), textureWidth(width), textureHeight(height) {
  sampler = std::move(glSampler);
}

void GLExternalTexture::updateTextureSize(int width, int height) {
  textureWidth = width;
  textureHeight = height;
}

Point GLExternalTexture::getTextureCoord(float x, float y) const {
  return {x / static_cast<float>(textureWidth), y / static_cast<float>(textureHeight)};
}

size_t GLExternalTexture::memoryUsage() const {
  return textureWidth * textureHeight * 3 / 2;
}

void GLExternalTexture::onReleaseGPU() {
  if (sampler.id > 0) {
    auto gl = tgfx::GLFunctions::Get(context);
    gl->deleteTextures(1, &sampler.id);
    sampler.id = 0;
  }
}
}  // namespace tgfx
