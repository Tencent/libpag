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

void GLContext::bindTexture(int unitIndex, const TextureSampler* sampler) {
  if (sampler == nullptr) {
    return;
  }
  auto glSampler = static_cast<const GLSampler*>(sampler);
  auto gl = glInterface->functions.get();
  gl->activeTexture(GL_TEXTURE0 + unitIndex);
  gl->bindTexture(glSampler->target, glSampler->id);
  gl->texParameteri(glSampler->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->texParameteri(glSampler->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl->texParameteri(glSampler->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->texParameteri(glSampler->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}
}  // namespace tgfx
