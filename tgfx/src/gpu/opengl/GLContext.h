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
#include "GLState.h"
#include "gpu/Context.h"

namespace tgfx {
class GLCaps;

class GLContext : public Context {
 public:
  static const GLInterface* Unwrap(Context* context) {
    return context ? static_cast<GLContext*>(context)->interface.get() : nullptr;
  }

  GLContext(Device* device, const GLInterface* glInterface);

  Backend backend() const override {
    return Backend::OPENGL;
  }

  const Caps* caps() const override {
    return interface->caps.get();
  }

 private:
  std::unique_ptr<const GLInterface> interface = nullptr;
  std::unique_ptr<GLState> glState = nullptr;

  friend class GLStateGuard;
  friend class GLDevice;
};
}  // namespace tgfx
