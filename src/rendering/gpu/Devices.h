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
 * back-reference from their texture handles, so Devices::CaptureFromTexture() may return nullptr
 * on
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
 * class, so adding a new backend is a matter of extending the Devices_* implementation files
 * rather than modifying render code.
 */
class Devices {
 public:
  /**
   * Creates a default device with no external resource constraint. Used by offscreen rendering,
   * the CLI, and internal helpers.
   *   OpenGL: GLDevice::MakeWithFallback()
   *   Metal:  MetalDevice::Make()
   *   Vulkan / D3D12 / WebGPU: Not implemented yet — selecting those backends fails at compile
   *           time (see Devices_Backend.cpp).
   */
  static std::shared_ptr<tgfx::Device> MakeDefault();

  /**
   * Creates an independent device for an asynchronous worker thread.
   *   OpenGL: Derives a share-context device from the current thread's GL context
   *           (GLDevice::Make(currentHandle)) so the worker can access resources created by the
   *           caller's GL context without contending on it.
   *   Other backends: Equivalent to MakeDefault(); those backends have thread-safe command
   *           encoding by design and require no per-thread device derivation.
   * Used by PAGDecoder.
   */
  static std::shared_ptr<tgfx::Device> MakeForAsyncThread();

  /**
   * Same as MakeForAsyncThread() but for a worker that renders into the given external backend
   * texture.
   *   OpenGL: Identical to MakeForAsyncThread(); OpenGL cannot walk from a texture id back to
   *           its owning context, so the worker device is still derived from the calling
   *           thread's current context. The texture parameter is unused on this backend but
   *           kept for signature parity.
   *   Metal:  Derives the device from the texture's own MTLDevice (via MakeForTexture) — Metal
   *           forbids using a texture on a different MTLDevice, so the texture's device is the
   *           only valid choice for the worker. Falls back to MakeDefault() when the texture
   *           does not carry a valid Metal texture handle.
   * Used by PAGSurface::MakeFrom(BackendTexture, forAsyncThread=true).
   */
  static std::shared_ptr<tgfx::Device> MakeForAsyncThread(const tgfx::BackendTexture& texture);

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
   *   Vulkan / D3D12 / WebGPU: Not implemented yet — selecting those backends fails at compile
   *                time (see Devices_Backend.cpp). Their texture types do not carry a device
   *                back-reference; a SetSharedDevice-style API is planned to lift that once the
   *                first of them lands.
   * Used by PAGImage::FromTexture() for implicit device inference.
   */
  static std::shared_ptr<tgfx::Device> MakeForTexture(const tgfx::BackendTexture& texture);

  /**
   * Same as MakeForTexture(BackendTexture) but for BackendRenderTarget. Used by
   * PAGSurface::MakeFrom(BackendRenderTarget) on backends that need to reach back through the
   * render target to locate the owning GPU device (Metal). GL adopts the current context as
   * usual; the remaining backends are not implemented yet (see Devices_Backend.cpp).
   */
  static std::shared_ptr<tgfx::Device> MakeForTexture(
      const tgfx::BackendRenderTarget& renderTarget);

  /**
   * Captures an identity tag for the device that owns the given external backend texture, to be
   * stored alongside the texture and verified later via CanSampleFrom().
   *   OpenGL: Records GLDevice::CurrentNativeHandle(); OpenGL cannot walk from a texture id back
   *           to its owning context, so the caller is expected to invoke this while the texture's
   *           creating context is current. The texture parameter is unused on this backend but
   *           kept for signature parity.
   *   Metal:  Reads MTLTexture.device and retains it in a backend-specific ExternalDeviceRef.
   *   Others: Return nullptr; VulkanImageInfo / WebGPUTextureInfo do not expose a device
   *           back-reference from the texture handle, and CanSampleFrom() will treat the null ref
   *           as "trust the caller".
   * Used by Picture::BackendTextureProxy to remember the external device identity.
   */
  static std::shared_ptr<ExternalDeviceRef> CaptureFromTexture(const tgfx::BackendTexture& texture);

  /**
   * Returns true when Devices::CaptureFromTexture() being nullptr is treated as an error condition
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
   * did not capture identity) the result is unconditionally true. Otherwise the render context's
   * device is compared against the captured one — a mismatch means the resource cannot be used
   * by that context and the caller must reject drawing.
   */
  static bool CanSampleFrom(tgfx::Context* context, const ExternalDeviceRef* deviceRef);

  /**
   * Creates a per-PAGSurface guard that protects the host GPU state around libpag rendering.
   *   OpenGL (Apple/Linux): Returns a GLRestorer that snapshots viewport / scissor / program /
   *           framebuffer binding / active texture / VAO / VBOs / blend equations and restores
   *           them.
   *   Others (including GL on Web and Windows): Returns nullptr — WebGL relies on emscripten's
   *           GL state management and Windows historically opted out of state preservation.
   *           Stateless command encoding (Metal/Vulkan/D3D12/WebGPU) cannot pollute host state
   *           either.
   * The returned guard is owned by PAGSurface for its full lifetime and reused across frames via
   * save() / restore(), avoiding per-frame heap allocation on the render hot path.
   */
  static std::unique_ptr<ExternalStateGuard> MakeExternalStateGuard();

  // Note: a user device injection API (SetSharedDevice or equivalent) is deliberately absent —
  // its lifetime and thread-safety contract will be defined together with the first backend
  // that actually needs it.
};

/**
 * Reentrancy-safe RAII lock over the default device.
 *
 * tgfx::Device::lockContext() uses a non-recursive mutex and only documents cross-thread
 * blocking; locking the same device twice from one thread is undefined and, with the device
 * deduplication introduced by tgfx #1581, deadlocks: MakeDefault() returns the same shared
 * Device instance to every caller on a thread, so a caller that already holds the device lock
 * and calls back into libpag code that fetches the default device (e.g. HTMLExporter::ToData
 * rasterizing gradient fills) re-enters the same mutex.
 *
 * DeviceLockScope removes that hazard for libpag-internal usage: scopes created on a thread
 * that already holds the lock (through an outer DeviceLockScope) reuse the live context and
 * only the outermost scope's destruction unlocks the device. Callers that bypass this class
 * and call tgfx::Device::lockContext() directly still own the reentrancy contract themselves.
 */
class DeviceLockScope {
 public:
  /**
   * Locks Devices::MakeDefault(), or reuses the already-locked default device when the calling
   * thread holds one through an outer DeviceLockScope. Use the bool conversion to check for
   * failure (device creation or context lock failed); the scope is then empty and inert.
   */
  DeviceLockScope();

  /**
   * Locks the given device instead of fetching the default one, so callers that cache a device
   * (e.g. GPUContext reusing one GL context across many rasterization calls) do not create a new
   * device per scope. Behaves exactly like the default constructor otherwise, including reusing
   * the context of an outer DeviceLockScope when the calling thread already holds one.
   */
  explicit DeviceLockScope(std::shared_ptr<tgfx::Device> device);

  /**
   * Releases the device lock, unless the calling thread still holds an outer DeviceLockScope
   * (the lock is released when the outermost scope is destroyed).
   */
  ~DeviceLockScope();

  DeviceLockScope(const DeviceLockScope&) = delete;
  DeviceLockScope& operator=(const DeviceLockScope&) = delete;

  /**
   * Returns the locked context, or nullptr when the scope failed to acquire one.
   */
  tgfx::Context* context() const {
    return _context;
  }

  /**
   * Returns false when the device could not be created or the context lock failed.
   */
  explicit operator bool() const {
    return _context != nullptr;
  }

 private:
  bool _ownsLock = false;
  tgfx::Context* _context = nullptr;
  std::shared_ptr<tgfx::Device> _device = {};
};

}  // namespace pag
