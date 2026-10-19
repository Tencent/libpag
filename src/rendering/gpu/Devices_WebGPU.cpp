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
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "rendering/gpu/Devices.h"

#if defined(TGFX_USE_WEBGPU)

#include "tgfx/gpu/webgpu/WebGPUDevice.h"

namespace pag {

std::shared_ptr<tgfx::Device> Devices::MakeDefault() {
  // WebGPUDevice::Make() reads the GPUDevice preinitialized on the JS side
  // (Module.preinitializedWebGPUDevice). It is only callable on the main runtime thread;
  // tgfx reports an error and returns nullptr otherwise.
  return tgfx::WebGPUDevice::Make();
}

bool Devices::SupportsBackend(tgfx::Backend backend) {
  return backend == tgfx::Backend::WebGPU;
}

std::shared_ptr<tgfx::Device> Devices::MakeForAsyncThread() {
  // WebGPU has no per-thread context to derive. Note that tgfx restricts device creation to
  // the main runtime thread, and the wasm bindings do not expose PAGDecoder (the only caller
  // of this method), so on Web this path is unreachable in practice.
  return MakeDefault();
}

std::shared_ptr<tgfx::Device> Devices::MakeForAsyncThread(const tgfx::BackendTexture&) {
  // WGPUTexture carries no device back-reference, so the worker device cannot be derived from
  // the texture. Web runs on a single shared GPUDevice, so the default device is correct.
  return MakeDefault();
}

Devices::AdoptedDevice Devices::AdoptCurrent() {
  // WebGPU has no thread-local "current device" to adopt. Return an empty AdoptedDevice so
  // that callers fall through to their own fallback (PAGSurfaceFactory falls back to
  // MakeForTexture / MakeDefault).
  return {};
}

std::shared_ptr<tgfx::Device> Devices::MakeForTexture(const tgfx::BackendTexture&) {
  // WGPUTexture does not expose its owning device. Returning nullptr lets PAGSurfaceFactory
  // fall back to MakeDefault(), which is correct for the single-device model on Web.
  return nullptr;
}

std::shared_ptr<tgfx::Device> Devices::MakeForTexture(const tgfx::BackendRenderTarget&) {
  // Same reasoning as the BackendTexture overload above.
  return nullptr;
}

std::shared_ptr<ExternalDeviceRef> Devices::CaptureFromTexture(const tgfx::BackendTexture&) {
  // WebGPUTextureInfo does not carry a device back-reference, so there is no identity to
  // capture. CanSampleFrom() treats the null ref as "trust the caller" — the host is required
  // to hand over textures created from the same GPUDevice that drives libpag rendering.
  return nullptr;
}

bool Devices::RequiresCapturedIdentity() {
  return false;
}

bool Devices::CanSampleFrom(tgfx::Context* context, const ExternalDeviceRef* deviceRef) {
  if (deviceRef == nullptr) {
    return true;
  }
  // A non-null ref cannot occur on this backend (CaptureFromTexture always returns nullptr),
  // but stay defensive: there is no device identity to compare, so fall back to checking that
  // a render context exists at all.
  return context != nullptr;
}

std::unique_ptr<ExternalStateGuard> Devices::MakeExternalStateGuard() {
  // WebGPU is stateless command encoding — there is no global GPU state to save/restore
  // around libpag rendering. Returning nullptr matches the Devices.h contract for non-GL
  // backends.
  return nullptr;
}

}  // namespace pag

#endif  // TGFX_USE_WEBGPU
