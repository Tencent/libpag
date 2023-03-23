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

#include "tgfx/utils/BytesKey.h"
#include "tgfx/core/Color.h"
#include "tgfx/gpu/Backend.h"
#include "tgfx/gpu/Caps.h"
#include "tgfx/gpu/Device.h"

namespace tgfx {
class ProgramCache;
class ResourceCache;
class DrawingManager;
class Gpu;
class ResourceProvider;
class ProxyProvider;
class Semaphore;

class Context {
 public:
  virtual ~Context();

  /**
   * Returns the associated device.
   */
  Device* device() const {
    return _device;
  }

  /**
   * Returns the associated cache that manages the lifetime of all Program instances.
   */
  ProgramCache* programCache() const {
    return _programCache;
  }

  /**
   * Returns the associated cache that manages the lifetime of all Resource instances.
   */
  ResourceCache* resourceCache() const {
    return _resourceCache;
  }

  DrawingManager* drawingManager() const {
    return _drawingManager;
  }

  ResourceProvider* resourceProvider() const {
    return _resourceProvider;
  }

  ProxyProvider* proxyProvider() const {
    return _proxyProvider;
  }

  /**
   * Returns the number of bytes consumed by internal gpu caches.
   */
  size_t memoryUsage() const;

  /**
   * Returns the number of bytes held by purgeable resources.
   */
  size_t purgeableBytes() const;

  /**
   * Returns current cache limits of max gpu memory byte size.
   */
  size_t getCacheLimit() const;

  /**
   * Sets the cache limit of max gpu memory byte size.
   */
  void setCacheLimit(size_t bytesLimit);

  /**
   * Purges GPU resources that haven't been used the passed in time.
   * @param purgeTime A timestamp previously returned by Clock::Now().
   * @param scratchResourcesOnly If true, the purgeable resources containing unique keys are spared.
   * If false, then all purgeable resources will be deleted.
   */
  void purgeResourcesNotUsedSince(int64_t purgeTime, bool scratchResourcesOnly = false);

  /**
   * Purge unreferenced resources from the cache until the provided bytesLimit has been reached, or
   * we have purged all unreferenced resources. Returns true if the total resource bytes is not over
   * the specified bytesLimit after purging.
   * @param bytesLimit The desired number of bytes after puring.
   * @param scratchResourcesOnly If true, the purgeable resources containing unique keys are spared.
   * If false, then all purgeable resources will be deleted.
   */
  bool purgeResourcesUntilMemoryTo(size_t bytesLimit, bool scratchResourcesOnly = false);

  /**
   * Inserts a GPU semaphore that the current GPU-backed API must wait on before executing any more
   * commands on the GPU for this surface. Surface will take ownership of the underlying semaphore
   * and delete it once it has been signaled and waited on. If this call returns false, then the
   * GPU back-end will not wait on the passed semaphore, and the client will still own the
   * semaphore. Returns true if GPU is waiting on the semaphore.
   */
  bool wait(const Semaphore* waitSemaphore);

  /**
   * Apply all pending changes to the render target immediately. After issuing all commands, the
   * semaphore will be signaled by the GPU. If the signalSemaphore is not null and uninitialized,
   * a new semaphore is created and initializes BackendSemaphore. The caller must delete the
   * semaphore returned in signalSemaphore. BackendSemaphore can be deleted as soon as this function
   * returns. If the back-end API is OpenGL, only uninitialized backend semaphores are supported.
   * If false is returned, the GPU back-end did not create or add a semaphore to signal on the GPU;
   * the caller should not instruct the GPU to wait on the semaphore.
   */
  bool flush(Semaphore* signalSemaphore = nullptr);

  /**
   * Submit outstanding work to the gpu from all previously un-submitted flushes. The return
   * value of the submit method will indicate whether the submission to the GPU was successful.
   *
   * If the call returns true, all previously passed in semaphores in flush calls will have been
   * submitted to the GPU and they can safely be waited on. The caller should wait on those
   * semaphores or perform some other global synchronization before deleting the semaphores.
   *
   * If it returns false, then those same semaphores will not have been submitted, and we will not
   * try to submit them again. The caller is free to delete the semaphores at any time.
   *
   * If the syncCpu flag is true, this function will return once the gpu has finished with all
   * submitted work.
   */
  bool submit(bool syncCpu = false);

  /**
   * Call to ensure all drawing to the context has been flushed and submitted to the underlying 3D
   * API. This is equivalent to calling Context::flush() followed by Context::submit(syncCpu).
   */
  void flushAndSubmit(bool syncCpu = false);

  /**
   * Returns the GPU backend of this context.
   */
  virtual Backend backend() const = 0;

  /**
   * Returns the capability info of this context.
   */
  virtual const Caps* caps() const = 0;

  /**
   * The Context normally assumes that no outsider is setting state within the underlying GPU API's
   * context/device/whatever. This call informs the context that the state was modified and it
   * should resend. Shouldn't be called frequently for good performance.
   */
  virtual void resetState() = 0;

  Gpu* gpu() {
    return _gpu;
  }

 protected:
  explicit Context(Device* device);

  Gpu* _gpu = nullptr;

 private:
  Device* _device = nullptr;
  ProgramCache* _programCache = nullptr;
  ResourceCache* _resourceCache = nullptr;
  DrawingManager* _drawingManager = nullptr;
  ResourceProvider* _resourceProvider = nullptr;
  ProxyProvider* _proxyProvider = nullptr;

  void releaseAll(bool releaseGPU);
  void onLocked();
  void onUnlocked();

  friend class Device;

  friend class Resource;
};

}  // namespace tgfx
