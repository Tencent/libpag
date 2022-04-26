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

#include "gpu/opengl/GLContext.h"
#include "gpu/opengl/GLUtil.h"
#include "tgfx/gpu/opengl/GLDevice.h"

namespace tgfx {
GLContext::GLContext(Device* device, const GLInterface* glInterface)
    : Context(device), glInterface(glInterface) {
}

void GLContext::resetState() {
}

static std::array<unsigned, 4> GetGLSwizzleValues(const Swizzle& swizzle) {
  unsigned glValues[4];
  for (int i = 0; i < 4; ++i) {
    switch (swizzle[i]) {
      case 'r':
        glValues[i] = GL_RED;
        break;
      case 'g':
        glValues[i] = GL_GREEN;
        break;
      case 'b':
        glValues[i] = GL_BLUE;
        break;
      case 'a':
        glValues[i] = GL_ALPHA;
        break;
      case '1':
        glValues[i] = GL_ONE;
        break;
      default:
        LOGE("Unsupported component");
    }
  }
  return {glValues[0], glValues[1], glValues[2], glValues[3]};
}

void GLContext::bindTexture(int unitIndex, const TextureSampler* sampler) {
  if (sampler == nullptr) {
    return;
  }
  auto glSampler = static_cast<const GLSampler*>(sampler);
  auto gl = glInterface->functions.get();
  auto caps = glInterface->caps.get();
  gl->activeTexture(GL_TEXTURE0 + unitIndex);
  gl->bindTexture(glSampler->target, glSampler->id);
  gl->texParameteri(glSampler->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->texParameteri(glSampler->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl->texParameteri(glSampler->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->texParameteri(glSampler->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  if (caps->textureSwizzleSupport) {
    const auto& swizzle = caps->getSwizzle(sampler->format);
    auto glValues = GetGLSwizzleValues(swizzle);
    if (caps->standard == GLStandard::GL) {
      gl->texParameteriv(glSampler->target, GL_TEXTURE_SWIZZLE_RGBA,
                         reinterpret_cast<const int*>(&glValues[0]));
    } else if (caps->standard == GLStandard::GLES) {
      // ES3 added swizzle support but not GL_TEXTURE_SWIZZLE_RGBA.
      gl->texParameteri(glSampler->target, GL_TEXTURE_SWIZZLE_R, static_cast<int>(glValues[0]));
      gl->texParameteri(glSampler->target, GL_TEXTURE_SWIZZLE_G, static_cast<int>(glValues[1]));
      gl->texParameteri(glSampler->target, GL_TEXTURE_SWIZZLE_B, static_cast<int>(glValues[2]));
      gl->texParameteri(glSampler->target, GL_TEXTURE_SWIZZLE_A, static_cast<int>(glValues[3]));
    }
  }
}
}  // namespace tgfx
