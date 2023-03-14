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

#include "opengl/GLSampler.h"
#include "GLCaps.h"
#include "opengl/GLUtil.h"

namespace tgfx {
std::unique_ptr<TextureSampler> TextureSampler::MakeFrom(Context* context,
                                                         const BackendTexture& backendTexture) {
  GLTextureInfo textureInfo = {};
  if (context == nullptr || !backendTexture.getGLTextureInfo(&textureInfo)) {
    return nullptr;
  }
  auto sampler = std::make_unique<GLSampler>();
  sampler->id = textureInfo.id;
  sampler->target = textureInfo.target;
  sampler->format = GLSizeFormatToPixelFormat(textureInfo.format);
  return sampler;
}

TextureType GLSampler::type() const {
  switch (target) {
    case GL_TEXTURE_2D:
      return TextureType::TwoD;
    case GL_TEXTURE_RECTANGLE:
      return TextureType::Rectangle;
    case GL_TEXTURE_EXTERNAL_OES:
      return TextureType::External;
    default:
      return TextureType::Unknown;
  }
}

BackendTexture GLSampler::getBackendTexture(int width, int height) const {
  GLTextureInfo textureInfo = {};
  textureInfo.id = id;
  textureInfo.target = target;
  textureInfo.format = PixelFormatToGLSizeFormat(format);
  return {textureInfo, width, height};
}

void GLSampler::computeKey(Context* context, BytesKey* bytesKey) const {
  auto caps = GLCaps::Get(context);
  bytesKey->write(static_cast<uint32_t>(caps->getReadSwizzle(format).asKey()));
  bytesKey->write(target);
}
}  // namespace tgfx