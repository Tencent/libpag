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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/opengl/egl/EGLWindow.h"

namespace pag {

// Destroys EGL window resources on a dedicated background thread. Some GPU drivers (e.g. PowerVR
// on MediaTek devices) block for a long time inside eglDestroyContext() while waiting for the GPU
// to drain. PAGSurface teardown is triggered from the main thread
// (TextureView.onSurfaceTextureDestroyed), so destroying the EGL resources inline can freeze the
// main thread and cause ANRs (issue #3685). Thread safety: EGL windows can only reach here after
// their PAGSurface is destroyed, which is serialized with all rendering by the rootLocker in
// PAGPlayer, so no thread is still using the EGL context. tgfx keeps its global device table
// behind a mutex and holds windows weakly in its present list, so the cross-thread destruction is
// safe even if another EGL context is current elsewhere.
//
// Tradeoff: the task queue is unbounded and every queued task holds strong references to its
// Surface/EGLWindow (i.e. the underlying EGLContext/EGLSurface/textures). On affected drivers each
// task with a window can block the worker for 35~85ms, so rapidly creating and destroying many
// PAGViews (e.g. fast list scrolling) can pile up N tasks and defer the reclamation of N sets of
// EGL resources by up to N * 85ms. This is deferred reclamation rather than a leak, but it can
// raise GPU memory pressure and, on drivers that cap the number of EGL contexts per process,
// temporarily hold more contexts than strictly necessary.
class EGLResourceDisposer {
 public:
  // Moves the cached Surface and its EGLWindow to the disposer thread. The Surface is released
  // first because it holds a strong reference to the window; the final window release runs
  // ~EGLDevice, where the potentially slow eglDestroyContext() executes off the main thread.
  static void DisposeAsync(std::shared_ptr<tgfx::Surface> surface,
                           std::shared_ptr<tgfx::EGLWindow> window);

 private:
  struct DisposeTask {
    std::shared_ptr<tgfx::Surface> surface = nullptr;
    std::shared_ptr<tgfx::EGLWindow> window = nullptr;
  };

  EGLResourceDisposer();

  static EGLResourceDisposer* GetInstance();

  void runLoop();

  std::mutex locker = {};
  std::condition_variable condition = {};
  std::queue<DisposeTask> tasks = {};
  std::thread workerThread = {};
};
}  // namespace pag
