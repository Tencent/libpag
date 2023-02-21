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

#include "GLVideoTexture.h"
#include <emscripten/val.h>
#include "gpu/Gpu.h"

namespace tgfx {
using namespace emscripten;

static constexpr int ANDROID_ALIGNMENT = 16;

std::shared_ptr<GLVideoTexture> GLVideoTexture::Make(Context* context, int width, int height) {
  static auto isAndroidMiniprogram =
      val::module_property("tgfx").call<bool>("isAndroidMiniprogram");
  auto sampler = context->gpu()->createSampler(width, height, PixelFormat::RGBA_8888, 0);
  if (sampler == nullptr) {
    return nullptr;
  }
  auto glSampler = static_cast<GLSampler*>(sampler.get());
  auto texture = Resource::Wrap(context, new GLVideoTexture(*glSampler, width, height));
  if (isAndroidMiniprogram) {
    // https://stackoverflow.com/questions/28291204/something-about-stagefright-codec-input-format-in-android
    // Video decoder will align to multiples of 16 on the Android WeChat mini-program.
    texture->textureWidth += ANDROID_ALIGNMENT - width % ANDROID_ALIGNMENT;
    texture->textureHeight += ANDROID_ALIGNMENT - height % ANDROID_ALIGNMENT;
  }
  return texture;
}

GLVideoTexture::GLVideoTexture(const GLSampler& glSampler, int width, int height)
    : GLTexture(width, height, SurfaceOrigin::TopLeft), textureWidth(width), textureHeight(height) {
  sampler = glSampler;
}

Point GLVideoTexture::getTextureCoord(float x, float y) const {
  return {x / static_cast<float>(textureWidth), y / static_cast<float>(textureHeight)};
}

size_t GLVideoTexture::memoryUsage() const {
  return width() * height() * 4;
}

void GLVideoTexture::onReleaseGPU() {
  context->gpu()->deleteSampler(&sampler);
}
}  // namespace tgfx
