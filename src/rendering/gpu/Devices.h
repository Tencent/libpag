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
 * Declares how an external resource will be used, so backends with explicit resource usage flags
 * can reject a description before it is imported instead of failing at draw time.
 */
enum class ExternalResourceUsage {
  Sampled,
  RenderTarget,
  SampledRenderTarget,
};

/**
 * Backend glue for libpag. CMake selects the implementation from the corresponding backend
 * directory, while callers only see the tgfx::Device base class. Adding a backend requires its
 * device implementation and platform integration without changing shared rendering code.
 */
class Devices {
 public:
  /**
   * Creates a default device with no external resource constraint. Used by offscreen rendering,
   * the CLI, and internal helpers.
   *   OpenGL: GLDevice::MakeWithFallback()
   *   Metal:  MetalDevice::Make()
*   WebGPU: WebGPUDevice::Make() — reads the GPUDevice preinitialized on the JS side; only
*           callable on the main runtime thread.
*   D3D12: D3D12Device from the default adapter, preferring hardware and falling back to WARP;
*           the registry keeps one tgfx device per native ID3D12Device.
*   Vulkan: Not implemented yet.
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
   *   D3D12:  Same as MakeForTexture(texture); the registry device is shared with the caller and
   *           access is serialized by its context mutex. There is no separate context.
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
   * Returns true if the given backend tag matches the backend this libpag build was compiled
   * with. Resource factories use this to reject backend-mismatched descriptions up front (e.g. a
   * GL BackendTexture handed to a Metal build) instead of returning objects that silently fail
   * to render at draw time.
   */
  static bool SupportsBackend(tgfx::Backend backend);

  /**
   * Creates a device that shares GPU resources with a caller-owned native context handle.
   *   OpenGL: GLDevice::Make(sharedContext) - the handle is a native GL context, so the new
   *           device joins its share group and both sides can see each other's textures. A null
   *           handle yields an independent device.
   *   D3D12: Returns nullptr for a non-null handle; a null handle returns MakeDefault().
   *   Other backends: Equivalent to MakeDefault(); they have no share-group notion tied to an
   *           opaque context handle, so the parameter is unused but kept for signature parity.
   * Used by pag_surface_make_offscreen_double_buffered(), whose sharedContext parameter predates
   * the multi-backend split and stays GL-shaped in the public C API.
   */
  static std::shared_ptr<tgfx::Device> MakeForSharedContext(void* sharedContext);

  /**
   * Returns true when the description carries this build's backend tag and all native
   * constraints for the requested usage are satisfied. Backends without a native description
   * only check the tag.
   */
  static bool ValidateExternalResource(const tgfx::BackendTexture& texture,
                                       ExternalResourceUsage usage);

  /**
   * Returns true when the render target carries this build's backend tag and all native
   * constraints for use as a render target are satisfied.
   */
  static bool ValidateExternalResource(const tgfx::BackendRenderTarget& renderTarget);

  /**
   * Validates a caller-owned GPU texture and resolves the device libpag must render it with.
   * Returns an empty AdoptedDevice when the description is invalid, fails the requested usage
   * constraints, or carries the tag of a backend this build was not compiled with; both cases
   * are logged, so resource factories only have to map a null device onto a null resource
   * instead of returning an object that can never be drawn. Otherwise the device is resolved as:
   *   forAsyncThread == true:  MakeForAsyncThread(texture); no default-device fallback.
   *   forAsyncThread == false: AdoptCurrent() — which sets externalContext on the backends that
   *                            have a thread-local current context — falling back to
   *                            MakeForTexture(texture) on the ones that do not.
   */
  static AdoptedDevice ResolveForExternalResource(const tgfx::BackendTexture& texture,
                                                  bool forAsyncThread, ExternalResourceUsage usage);

  /**
   * Same as ResolveForExternalResource(BackendTexture, bool) for a caller-owned render target.
   * Render targets are never resolved for an async worker, so there is no forAsyncThread variant.
   */
  static AdoptedDevice ResolveForExternalResource(const tgfx::BackendRenderTarget& renderTarget);

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
   *   WebGPU:      Returns MakeDefault(); WGPUTexture carries no device back-reference, but Web
   *                runs on a single shared GPUDevice, so the default device is always the one
   *                that created the caller's texture.
   *   D3D12:       Reads ID3D12Resource::GetDevice() and returns the registry device for that
   *                native device, creating it on first use.
   *   Vulkan:      Not implemented yet.
   * Used by PAGImage::FromTexture() for implicit device inference.
   */
  static std::shared_ptr<tgfx::Device> MakeForTexture(const tgfx::BackendTexture& texture);

  /**
   * Same as MakeForTexture(BackendTexture) but for BackendRenderTarget. Used by
   * PAGSurface::MakeFrom(BackendRenderTarget) on backends that need to reach back through the
   * render target to locate the owning GPU device (Metal). GL adopts the current context as
   * usual; WebGPU returns MakeDefault() since Web runs on a single shared GPUDevice; D3D12
   * resolves through the render target's resource.
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
   *   D3D12:  Retains the ID3D12Device identity from the resource.
   *   Vulkan / WebGPU: Return nullptr; their texture handles expose no device back-reference, and
   *           CanSampleFrom() treats the null ref as "trust the caller".
   * Used by Picture::BackendTextureProxy to remember the external device identity.
   */
  static std::shared_ptr<ExternalDeviceRef> CaptureFromTexture(const tgfx::BackendTexture& texture);

  /**
   * Returns true when Devices::CaptureFromTexture() being nullptr is treated as an error condition
   * for the current backend, false otherwise. OpenGL (thread-local current context) and D3D12
   * (native device identity) return true; Metal / Vulkan / WebGPU have no such requirement, so a
   * null capture is normal and must not fail the caller.
   */
  static bool RequiresCapturedIdentity();

  /**
   * Returns true when `context` can safely sample a resource whose device identity was previously
   * captured as `deviceRef`. OpenGL, Metal, Vulkan and WebGPU treat a null deviceRef as trusting
   * the caller. D3D12 requires a captured identity and rejects a null deviceRef; callers use
   * RequiresCapturedIdentity() to reject failed captures before drawing. For a non-null ref the
   * render context's device is compared against the captured identity, and a mismatch prevents
   * the resource from being sampled.
   */
  static bool CanSampleFrom(tgfx::Context* context, const ExternalDeviceRef* deviceRef);

  /**
   * Submits pending work and waits for the backend's completion primitive, returning the device
   * health sampled before the wait. Backends without a synchronous fence only submit.
   */
  static bool WaitForQueue(tgfx::Context* context);

  /**
   * Tracks shared ownership for the D3D12 context identity cache without retaining the device.
   * Other backends do not need a context identity cache and leave this operation empty.
   */
  static void TrackContextDevice(const std::shared_ptr<tgfx::Device>& device);

  /**
   * Locks the device and records a permanent wrapper failure when locking returns null. Main-thread
   * callers of tgfx::Device::lockContext() should use this entry point so later health checks can
   * avoid handing the same unusable device back to the caller.
   */
  static tgfx::Context* LockContext(const std::shared_ptr<tgfx::Device>& device);

#if defined(TGFX_USE_D3D12)
  /**
   * Returns the number of context identity records whose tracked owner is still alive.
   */
  static size_t GetContextIdentityCacheCountForTesting();

  /**
   * Returns the number of native identity capture attempts made through a temporary texture.
   */
  static size_t GetIdentityCaptureCountForTesting();

  /**
   * Returns the number of successful tgfx device creations performed by the D3D12 registry.
   */
  static size_t GetDeviceCreationCountForTesting();

  /**
   * Returns the number of live devices currently published in the D3D12 registry. Intended for
   * tests that verify concurrent imports converge on a single device.
   */
  static size_t GetRegisteredDeviceCount();

  using DeviceHealthProbe = long (*)(void* nativeDevice);

  /**
   * Overrides the native device health query for tests and returns the previous probe. Pass nullptr
   * to restore ID3D12Device::GetDeviceRemovedReason(). The callback must not acquire device locks.
   */
  static DeviceHealthProbe SetDeviceHealthProbeForTesting(DeviceHealthProbe probe);
#endif

  /**
   * Checks backend device health while the caller holds the context lock. D3D12 checks native
   * removal and permanent lock failures already observed by LockContext(); it cannot inspect
   * unobserved internal tgfx failures. Backends without a native health query return true for a
   * non-null context.
   */
  static bool IsDeviceHealthy(tgfx::Context* context);

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
 * RAII boundary lock that keeps a GPU device alive and unlocks it on destruction after a successful
 * lockContext(). The lock is non-recursive: the calling thread must not already hold the same
 * device's context lock. Construct and destroy the scope on the same thread, and pass context()
 * to work performed within the scope instead of acquiring the device lock again.
 * Callers must release the lock before invoking libpag APIs that manage device locks internally.
 */
class DeviceLockScope {
 public:
  /**
   * Creates and locks a default device. The scope is empty if device creation or context locking
   * fails; use the bool conversion to check for success.
   */
  DeviceLockScope();

  /**
   * Locks the given device and retains it for the scope's lifetime. The scope is empty if the
   * device is null or context locking fails. The calling thread must not already hold its lock.
   */
  explicit DeviceLockScope(std::shared_ptr<tgfx::Device> device);

  /**
   * Releases the device lock if this scope acquired it successfully.
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
  tgfx::Context* _context = nullptr;
  std::shared_ptr<tgfx::Device> _device = {};
};

}  // namespace pag
