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
 * libpag builds without ARC, so ownership is managed manually: retain in the constructor and
 * release in the destructor, which keeps the object alive for the lifetime of the ref.
 */
class MetalExternalDeviceRef : public ExternalDeviceRef {
 public:
  explicit MetalExternalDeviceRef(id<MTLDevice> mtlDevice) : device([mtlDevice retain]) {
  }

  ~MetalExternalDeviceRef() {
    [device release];
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

std::shared_ptr<tgfx::Device> Devices::MakeForAsyncThread(const tgfx::BackendTexture& texture) {
  // The async worker renders into the caller's external texture, whose own MTLDevice is the only
  // device allowed to touch it. Fall back to the default device when the texture does not carry
  // a valid Metal texture handle.
  auto device = MakeForTexture(texture);
  if (device == nullptr) {
    device = MakeDefault();
  }
  return device;
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
  // MetalDevice::MakeFrom deduplicates by id<MTLDevice> inside tgfx, so surfaces built from
  // textures on the same GPU share one Device (and its ResourceCache / shader cache).
  return tgfx::MetalDevice::MakeFrom(mtlDevice);
}

std::shared_ptr<tgfx::Device> Devices::MakeForTexture(
    const tgfx::BackendRenderTarget& renderTarget) {
  // Same reasoning as the BackendTexture overload — Metal render targets are just MTLTextures
  // under the hood, so ask the texture for its device.
  tgfx::MetalTextureInfo mtlInfo = {};
  if (!renderTarget.getMetalTextureInfo(&mtlInfo) || mtlInfo.texture == nullptr) {
    return nullptr;
  }
  id<MTLTexture> mtlTexture = (__bridge id<MTLTexture>)mtlInfo.texture;
  id<MTLDevice> mtlDevice = mtlTexture.device;
  if (mtlDevice == nil) {
    return nullptr;
  }
  return tgfx::MetalDevice::MakeFrom(mtlDevice);
}

std::shared_ptr<ExternalDeviceRef> Devices::CaptureFromTexture(
    const tgfx::BackendTexture& texture) {
  // Metal can reach back through the MTLTexture to its owning MTLDevice. Retaining the device in
  // MetalExternalDeviceRef keeps the identity valid for the render-time comparison even after the
  // caller drops its own reference to the texture.
  tgfx::MetalTextureInfo mtlInfo = {};
  if (!texture.getMetalTextureInfo(&mtlInfo) || mtlInfo.texture == nullptr) {
    return nullptr;
  }
  id<MTLTexture> mtlTexture = (__bridge id<MTLTexture>)mtlInfo.texture;
  id<MTLDevice> mtlDevice = mtlTexture.device;
  if (mtlDevice == nil) {
    return nullptr;
  }
  return std::make_shared<MetalExternalDeviceRef>(mtlDevice);
}

bool Devices::RequiresCapturedIdentity() {
  return false;
}

bool Devices::CanSampleFrom(tgfx::Context* context, const ExternalDeviceRef* deviceRef) {
  if (deviceRef == nullptr) {
    return true;
  }
  if (context == nullptr) {
    return false;
  }
  // Metal forbids using a resource on a different MTLDevice (API validation assert or undefined
  // behavior when validation is off), so a mismatch must reject drawing instead of failing at
  // command encode time.
  auto metalDevice = static_cast<tgfx::MetalDevice*>(context->device());
  auto metalRef = static_cast<const MetalExternalDeviceRef*>(deviceRef);
  auto contextDevice = (id<MTLDevice>)metalDevice->metalDevice();
  return contextDevice == metalRef->device;
}

std::unique_ptr<ExternalStateGuard> Devices::MakeExternalStateGuard() {
  // Metal is stateless command encoding — there is no global GPU state to save/restore around
  // libpag rendering. Returning nullptr matches the Devices.h contract for non-GL backends.
  return nullptr;
}

}  // namespace pag

#endif  // TGFX_USE_METAL
