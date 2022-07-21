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

#pragma once

#include "GLInterface.h"
#include "gpu/SamplerState.h"
#include "tgfx/gpu/Context.h"
#include "tgfx/gpu/opengl/GLSampler.h"

namespace tgfx {
class GLCaps;

class GLContext : public Context {
 public:
  static GLContext* Unwrap(Context* context) {
    return static_cast<GLContext*>(context);
  }

  GLContext(Device* device, const GLInterface* glInterface);

  Backend backend() const override {
    return Backend::OPENGL;
  }

  const GLFunctions* functions() const {
    return glInterface->functions.get();
  }

  const Caps* caps() const override {
    return glInterface->caps.get();
  }

  void resetState() override;

  void bindTexture(int unitIndex, const TextureSampler* sampler, SamplerState sampleState = {});

 private:
  const GLInterface* glInterface = nullptr;

  friend class GLDevice;
  friend class GLInterface;
};
}  // namespace tgfx
