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

#include "tgfx/gpu/Window.h"
#include "core/utils/Log.h"
#include "tgfx/gpu/Device.h"

namespace tgfx {
Window::Window(std::shared_ptr<Device> device) : device(std::move(device)) {
}

std::shared_ptr<Surface> Window::createSurface(Context* context) {
  if (!checkContext(context)) {
    return nullptr;
  }
  return onCreateSurface(context);
}

void Window::present(Context* context, int64_t presentationTime) {
  if (!checkContext(context)) {
    return;
  }
  onPresent(context, presentationTime);
}

bool Window::checkContext(Context* context) {
  if (context == nullptr) {
    return false;
  }
  if (context->device() != device.get()) {
    LOGE("Window::checkContext() : context is not locked from the same device of this window");
    return false;
  }
  return true;
}
}  // namespace tgfx