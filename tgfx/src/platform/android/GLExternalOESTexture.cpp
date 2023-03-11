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

#include "GLExternalOESTexture.h"
#include "gpu/Gpu.h"
#include "opengl/GLSampler.h"
#include "tgfx/opengl/GLFunctions.h"

namespace tgfx {
std::shared_ptr<GLExternalOESTexture> GLExternalOESTexture::Make(Context* context, int width,
                                                                 int height) {
  if (context == nullptr || width < 1 || height < 1) {
    return nullptr;
  }
  auto gl = tgfx::GLFunctions::Get(context);
  auto sampler = std::make_unique<GLSampler>();
  sampler->target = GL_TEXTURE_EXTERNAL_OES;
  sampler->format = tgfx::PixelFormat::RGBA_8888;
  gl->genTextures(1, &sampler->id);
  if (sampler->id == 0) {
    return nullptr;
  }
  return Resource::Wrap(context, new GLExternalOESTexture(std::move(sampler), width, height));
}

GLExternalOESTexture::GLExternalOESTexture(std::unique_ptr<TextureSampler> sampler, int width,
                                           int height)
    : Texture(width, height, ImageOrigin::TopLeft),
      sampler(std::move(sampler)),
      textureWidth(width),
      textureHeight(height) {
}

void GLExternalOESTexture::updateTextureSize(int width, int height) {
  textureWidth = width;
  textureHeight = height;
}

Point GLExternalOESTexture::getTextureCoord(float x, float y) const {
  return {x / static_cast<float>(textureWidth), y / static_cast<float>(textureHeight)};
}

BackendTexture GLExternalOESTexture::getBackendTexture() const {
  return getSampler()->getBackendTexture(textureWidth, textureHeight);
}

size_t GLExternalOESTexture::memoryUsage() const {
  return textureWidth * textureHeight * 3 / 2;
}

void GLExternalOESTexture::onReleaseGPU() {
  context->gpu()->deleteSampler(sampler.get());
}
}  // namespace tgfx
