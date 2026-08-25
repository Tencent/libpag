/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include "rendering/gpu/Devices.h"

#if defined(TGFX_USE_OPENGL)

#include "rendering/gpu/GLRestorer.h"
#include "tgfx/gpu/Context.h"
#include "tgfx/gpu/opengl/GLDevice.h"

namespace pag {

namespace {

/**
 * ExternalDeviceRef subclass for the OpenGL backend. Stores the native GL context handle captured
 * at Devices::CaptureCurrent() time. The handle is compared via GLDevice::sharableWith() on the
 * render thread; libpag never dereferences it, so no retain is needed.
 */
class GLExternalDeviceRef : public ExternalDeviceRef {
 public:
  explicit GLExternalDeviceRef(void* handle) : nativeHandle(handle) {
  }

  void* nativeHandle = nullptr;
};

}  // namespace

std::shared_ptr<tgfx::Device> Devices::MakeDefault() {
  return tgfx::GLDevice::MakeWithFallback();
}

std::shared_ptr<tgfx::Device> Devices::MakeForAsyncThread() {
  auto sharedContext = tgfx::GLDevice::CurrentNativeHandle();
  if (sharedContext == nullptr) {
    // No current GL context on this thread: nothing to derive from. The caller decides how to
    // proceed (some call sites fall back to Devices::MakeDefault(), others pass the nullptr
    // through to let an internal component's own default-device fallback kick in).
    return nullptr;
  }
  return tgfx::GLDevice::Make(sharedContext);
}

Devices::AdoptedDevice Devices::AdoptCurrent() {
  auto device = tgfx::GLDevice::Current();
  if (device == nullptr) {
    return {};
  }
  return {std::move(device), true};
}

std::shared_ptr<tgfx::Device> Devices::MakeForTexture(const tgfx::BackendTexture&) {
  // GL cannot walk from a texture id back to its owning context. The caller is expected to invoke
  // this while the texture's creating context is current on the calling thread.
  return tgfx::GLDevice::Current();
}

std::shared_ptr<ExternalDeviceRef> Devices::CaptureCurrent() {
  auto handle = tgfx::GLDevice::CurrentNativeHandle();
  if (handle == nullptr) {
    return nullptr;
  }
  return std::make_shared<GLExternalDeviceRef>(handle);
}

bool Devices::RequiresCapturedIdentity() {
  return true;
}

bool Devices::CanSampleFrom(tgfx::Context* context, const ExternalDeviceRef* deviceRef) {
  if (deviceRef == nullptr) {
    return true;
  }
  if (context == nullptr) {
    return false;
  }
  auto glDevice = static_cast<tgfx::GLDevice*>(context->device());
  auto glRef = static_cast<const GLExternalDeviceRef*>(deviceRef);
  return glDevice->sharableWith(glRef->nativeHandle);
}

std::unique_ptr<ExternalStateGuard> Devices::MakeExternalStateGuard() {
#if !defined(PAG_BUILD_FOR_WEB) && !defined(_WIN32)
  return std::make_unique<GLRestorer>();
#else
  // Web builds rely on emscripten's GL state management; Windows historically opted out of state
  // preservation and this refactor keeps that behavior. See docs/gpu-backend-decoupling.md §8.1.
  return nullptr;
#endif
}

}  // namespace pag

#endif  // TGFX_USE_OPENGL
