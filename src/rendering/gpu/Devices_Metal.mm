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

#if defined(TGFX_USE_METAL)

#import <Metal/Metal.h>
#include "tgfx/gpu/Context.h"
#include "tgfx/gpu/metal/MetalDevice.h"

namespace pag {

namespace {

/**
 * ExternalDeviceRef subclass for the Metal backend. Retains the underlying id<MTLDevice> so
 * comparing identity later is safe even after the caller drops its own strong reference.
 * Under ARC (which libpag builds with on Apple), the strong ivar keeps the object alive for the
 * lifetime of the ref.
 */
class MetalExternalDeviceRef : public ExternalDeviceRef {
 public:
  explicit MetalExternalDeviceRef(id<MTLDevice> mtlDevice) : device(mtlDevice) {
  }

  id<MTLDevice> device = nil;
};

}  // namespace

std::shared_ptr<tgfx::Device> Devices::MakeDefault() {
  return tgfx::MetalDevice::Make();
}

std::shared_ptr<tgfx::Device> Devices::MakeForAsyncThread() {
  // Metal has no "current context" concept. Command encoders are thread-safe by design, so an
  // async worker can just use a fresh default device. Any resource sharing with the caller must
  // be handled explicitly by matching MTLDevices at texture construction time — see
  // MakeForTexture below.
  return tgfx::MetalDevice::Make();
}

Devices::AdoptedDevice Devices::AdoptCurrent() {
  // Metal has no thread-local "current device" to adopt. Return an empty AdoptedDevice so that
  // callers wanting an external-context device fall through to whatever fallback they define
  // (PAGSurfaceFactory falls back to MakeForAsyncThread / MakeDefault).
  return {};
}

std::shared_ptr<tgfx::Device> Devices::MakeForTexture(const tgfx::BackendTexture& texture) {
  // Reach into the MTLTexture the caller passed in and reuse its own MTLDevice. This is the only
  // way to guarantee that libpag's render surface shares GPU resources with the external texture
  // — MetalDevice::MakeFrom() will wrap the same id<MTLDevice> and return a Device that talks to
  // the same MTLCommandQueue family.
  tgfx::MetalTextureInfo mtlInfo = {};
  if (!texture.getMetalTextureInfo(&mtlInfo) || mtlInfo.texture == nullptr) {
    return nullptr;
  }
  id<MTLTexture> mtlTexture = (__bridge id<MTLTexture>)mtlInfo.texture;
  id<MTLDevice> mtlDevice = mtlTexture.device;
  if (mtlDevice == nil) {
    return nullptr;
  }
  return tgfx::MetalDevice::MakeFrom((__bridge void*)mtlDevice);
}

std::shared_ptr<ExternalDeviceRef> Devices::CaptureCurrent() {
  // Metal has no thread-local "current device", so there is nothing to capture. Returning
  // nullptr is the intended sentinel — CanSampleFrom() treats a null ref as "trust the caller",
  // and RequiresCapturedIdentity() returns false so higher-level factories (PAGImage::FromTexture,
  // Picture::MakeFrom) do not treat this as an error.
  return nullptr;
}

bool Devices::RequiresCapturedIdentity() {
  return false;
}

bool Devices::CanSampleFrom(tgfx::Context* /*context*/, const ExternalDeviceRef* /*deviceRef*/) {
  // Metal cannot walk from a tgfx::Context back to the MTLDevice in a portable way without
  // reaching into tgfx internals. Since MakeForTexture already forced the render Device to be
  // built from the external texture's own MTLDevice, sampling is safe by construction. Return
  // true unconditionally; if a future test needs a stronger check, MetalExternalDeviceRef.device
  // can be compared against the Context's MetalDevice once tgfx exposes that.
  return true;
}

std::unique_ptr<ExternalStateGuard> Devices::MakeExternalStateGuard() {
  // Metal is stateless command encoding — there is no global GPU state to save/restore around
  // libpag rendering. Returning nullptr matches the Devices.h contract for non-GL backends.
  return nullptr;
}

}  // namespace pag

#endif  // TGFX_USE_METAL
