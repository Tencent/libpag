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

#pragma once

#include <memory>
#include "tgfx/gpu/Backend.h"
#include "tgfx/gpu/Device.h"

namespace pag {

/**
 * Abstract guard that saves and restores host GPU global state around libpag rendering, so a
 * PAGSurface rendered into a caller-owned GPU context does not disturb the caller's state.
 * The guard is created once per PAGSurface and reused across frames; save() is called on each
 * lockContext() and restore() on each unlockContext(), avoiding per-frame heap allocation.
 * Only the OpenGL backend produces a real implementation. Metal / Vulkan / D3D12 / WebGPU use
 * stateless command recording and do not need this abstraction; Devices::MakeExternalStateGuard()
 * returns nullptr for those backends.
 */
class ExternalStateGuard {
 public:
  virtual ~ExternalStateGuard() = default;

  /**
   * Captures the current host GPU global state so it can be restored later by restore().
   */
  virtual void save(tgfx::Context* context) = 0;

  /**
   * Restores the GPU state captured by the most recent save() call.
   */
  virtual void restore() = 0;
};

/**
 * Opaque identity tag for an external GPU resource, used to verify that a rendering context can
 * safely sample the resource (i.e. shares GPU resources with the device that created it).
 * Each backend subclass holds the underlying handle with strong ownership so the identity is not
 * dangling after the creator releases its own reference. Vulkan and WebGPU do not expose a device
 * back-reference from their texture handles, so Devices::CaptureCurrent() may return nullptr on
 * those backends and Devices::CanSampleFrom() treats a null ref as "trust the caller".
 */
class ExternalDeviceRef {
 public:
  virtual ~ExternalDeviceRef() = default;
};

/**
 * Backend glue for libpag. Every call into a concrete tgfx backend (OpenGL, Metal, Vulkan, D3D12,
 * WebGPU) is funneled through this class and dispatched via compile-time macros
 * (TGFX_USE_OPENGL / TGFX_USE_METAL / ...). The rest of libpag only sees the tgfx::Device base
 * class, so adding a new backend is a matter of extending Devices.cpp rather than modifying
 * render code.
 */
class Devices {
 public:
  /**
   * Creates a default device with no external resource constraint. Used by offscreen rendering,
   * the CLI, and internal helpers.
   *   OpenGL: GLDevice::MakeWithFallback()
   *   Metal:  MetalDevice::Make()
   *   Vulkan: VulkanDevice::Make()
   *   D3D12:  D3D12Device::Make() or MakeWarp()
   *   WebGPU: WebGPUDevice::Make()
   */
  static std::shared_ptr<tgfx::Device> MakeDefault();

  /**
   * Creates an independent device for an asynchronous worker thread.
   *   OpenGL: Derives a share-context device from the current thread's GL context
   *           (GLDevice::Make(currentHandle)) so the worker can access resources created by the
   *           caller's GL context without contending on it.
   *   Other backends: Equivalent to MakeDefault(); those backends have thread-safe command
   *           encoding by design and require no per-thread device derivation.
   * Used by PAGDecoder and by PAGSurface::MakeFrom(BackendTexture, forAsyncThread=true).
   */
  static std::shared_ptr<tgfx::Device> MakeForAsyncThread();

  /**
   * A device that was "adopted" from the caller's environment, together with a flag indicating
   * whether the caller retains ownership of the underlying GPU context. When externalContext is
   * true, PAGSurface must protect the host GPU state via ExternalStateGuard while rendering.
   */
  struct AdoptedDevice {
    std::shared_ptr<tgfx::Device> device;
    bool externalContext = false;
  };

  /**
   * Adopts the host thread's "current" GPU context as a rendering device. Only meaningful on the
   * OpenGL backend, where {GLDevice::Current(), externalContext=true} is returned so the caller
   * keeps ownership of the GL context and libpag guards its global state. Other backends return
   * {nullptr, false} because they do not expose a thread-local "current" device.
   * Used by PAGSurface::MakeFrom(BackendRenderTarget) and PAGSurface::MakeFrom(BackendTexture,
   * forAsyncThread=false).
   */
  static AdoptedDevice AdoptCurrent();

  /**
   * Creates a device compatible with sampling the given external backend texture.
   *   OpenGL:      Falls back to GLDevice::Current(); OpenGL cannot walk from a texture id back
   *                to its owning context, so the caller is expected to invoke this while the
   *                texture's creating context is current. The texture parameter is unused on this
   *                backend but kept for signature parity.
   *   Metal:       Reads MTLTexture.device and wraps it via MetalDevice::MakeFrom().
   *   D3D12:       Queries ID3D12Resource::GetDevice() and wraps it via D3D12Device::MakeFrom().
   *   Vulkan/WebGPU: Falls back to MakeDefault(); VulkanImageInfo / WebGPUTextureInfo do not carry
   *                a device back-reference, so the caller is expected to have created the texture
   *                on the same device libpag uses internally (a future SetSharedDevice-style API
   *                will lift this restriction).
   * Used by PAGImage::FromTexture() for implicit device inference.
   */
  static std::shared_ptr<tgfx::Device> MakeForTexture(const tgfx::BackendTexture& texture);

  /**
   * Captures an identity tag for the calling thread's current host GPU context, to be stored
   * alongside an external resource and verified later via CanSampleFrom().
   *   OpenGL: Records GLDevice::CurrentNativeHandle().
   *   Others: Return nullptr; those backends have no thread-local "current" concept and
   *           CanSampleFrom() will treat the null ref as "trust the caller".
   * Used by Picture::BackendTextureProxy to remember the external device identity.
   */
  static std::shared_ptr<ExternalDeviceRef> CaptureCurrent();

  /**
   * Returns true when Devices::CaptureCurrent() being nullptr is treated as an error condition
   * for the current backend, false otherwise. Only OpenGL (which has a thread-local "current
   * context" concept) returns true — callers such as PAGImage::FromTexture use this to decide
   * whether a null capture should be reported as a missing GPU context or accepted silently.
   * Metal / Vulkan / D3D12 / WebGPU have no thread-local context, so a null capture is normal
   * and must not fail the caller.
   */
  static bool RequiresCapturedIdentity();

  /**
   * Returns true when `context` can safely sample a resource whose device identity was previously
   * captured as `deviceRef`. When deviceRef is null (Vulkan/WebGPU capture, or a call site that
   * did not capture identity) the result is unconditionally true.
   */
  static bool CanSampleFrom(tgfx::Context* context, const ExternalDeviceRef* deviceRef);

  /**
   * Creates a per-PAGSurface guard that protects the host GPU state around libpag rendering.
   *   OpenGL: Returns a GLRestorer that snapshots viewport / scissor / program / framebuffer
   *           binding / active texture / VAO / VBOs / blend equations and restores them.
   *   Others: Returns nullptr; stateless command encoding cannot pollute host state.
   * The returned guard is owned by PAGSurface for its full lifetime and reused across frames via
   * save() / restore(), avoiding per-frame heap allocation on the render hot path.
   */
  static std::unique_ptr<ExternalStateGuard> MakeExternalStateGuard();

  // Note: SetSharedDevice (or an equivalent user device injection API) is deliberately absent.
  // It is a process-wide mutable global with backend-dependent semantics (GL: derive from
  // injected device; Metal/Vulkan/D3D12/WebGPU: reuse directly) and is not exercised by any test
  // in this refactor. It will be introduced together with the first backend that actually needs
  // it (Vulkan or WebGPU), so its lifetime and thread-safety contract can be defined against a
  // real caller instead of speculatively. See docs/gpu-backend-decoupling.md §3.8.
};

}  // namespace pag
