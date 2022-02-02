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

#include "GLDevice.h"
#include "GLContext.h"
#include "gpu/opengl/GLUtil.h"

namespace pag {
static std::mutex deviceMapLocker = {};
static std::unordered_map<void*, GLDevice*> deviceMap = {};

std::shared_ptr<GLDevice> GLDevice::Get(void* nativeHandle) {
  if (nativeHandle == nullptr) {
    return nullptr;
  }
  std::lock_guard<std::mutex> autoLock(deviceMapLocker);
  auto result = deviceMap.find(nativeHandle);
  if (result != deviceMap.end()) {
    auto device = result->second->weakThis.lock();
    if (device) {
      return std::static_pointer_cast<GLDevice>(device);
    }
    deviceMap.erase(result);
  }
  return nullptr;
}

GLDevice::GLDevice(void* nativeHandle) : nativeHandle(nativeHandle) {
  std::lock_guard<std::mutex> autoLock(deviceMapLocker);
  deviceMap[nativeHandle] = this;
}

GLDevice::~GLDevice() {
  std::lock_guard<std::mutex> autoLock(deviceMapLocker);
  deviceMap.erase(nativeHandle);
}

bool GLDevice::onLockContext() {
  if (!onMakeCurrent()) {
    return false;
  }
  if (context == nullptr) {
    auto glInterface = GLInterface::GetNative();
    if (glInterface != nullptr) {
      context = new GLContext(this, glInterface);
    } else {
      LOGE("GLDevice::onLockContext(): Error on creating GLInterface! ");
    }
  }
  if (context == nullptr) {
    onClearCurrent();
    return false;
  }
  auto glContext = static_cast<GLContext*>(context);
  if (isAdopted) {
    glContext->glState->reset();
    glContext->glState->save();
    // Clear externally generated GLError.
    CheckGLError(glContext->interface.get());
  }
  return true;
}

void GLDevice::onUnlockContext() {
  auto glContext = static_cast<GLContext*>(context);
  if (isAdopted) {
    glContext->glState->restore();
  }
  onClearCurrent();
}
}  // namespace pag