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

#include "EGLResourceDisposer.h"

namespace pag {

EGLResourceDisposer* EGLResourceDisposer::GetInstance() {
  // Intentional leak: the disposer and its worker thread live for the entire process. A static
  // destructor would join the thread during teardown when the EGL display may already be gone,
  // and Android never unloads the library once loaded.
  static auto* instance = new EGLResourceDisposer();
  return instance;
}

EGLResourceDisposer::EGLResourceDisposer() {
  workerThread = std::thread(&EGLResourceDisposer::runLoop, this);
}

void EGLResourceDisposer::DisposeAsync(std::shared_ptr<tgfx::Surface> surface,
                                       std::shared_ptr<tgfx::EGLWindow> window) {
  if (surface == nullptr && window == nullptr) {
    return;
  }
  auto disposer = GetInstance();
  {
    std::lock_guard<std::mutex> autoLock(disposer->locker);
    auto& task = disposer->tasks.emplace();
    task.surface = std::move(surface);
    task.window = std::move(window);
  }
  disposer->condition.notify_one();
}

void EGLResourceDisposer::runLoop() {
  while (true) {
    DisposeTask task = {};
    {
      std::unique_lock<std::mutex> autoLock(locker);
      while (tasks.empty()) {
        condition.wait(autoLock);
      }
      task = std::move(tasks.front());
      tasks.pop();
    }
    // Reset the surface first: it holds a strong reference to the window, and dropping both here
    // keeps the whole ~EGLWindow -> ~EGLDevice -> eglDestroyContext() chain on this thread.
    task.surface = nullptr;
    task.window = nullptr;
  }
}
}  // namespace pag
