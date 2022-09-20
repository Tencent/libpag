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

#include "DrawingManager.h"
#include "TextureResolveRenderTask.h"
#include "gpu/Gpu.h"

namespace tgfx {
void DrawingManager::closeActiveOpsTask() {
  if (activeOpsTask) {
    activeOpsTask->makeClosed();
    activeOpsTask = nullptr;
  }
}

void DrawingManager::newTextureResolveRenderTask(Surface* surface) {
  if (surface->renderTarget->sampleCount() <= 1 || !surface->requiresManualMSAAResolve) {
    return;
  }
  closeActiveOpsTask();
  auto task = std::make_shared<TextureResolveRenderTask>(surface->renderTarget);
  task->makeClosed();
  tasks.push_back(std::move(task));
}

std::shared_ptr<OpsTask> DrawingManager::newOpsTask(Surface* surface) {
  closeActiveOpsTask();
  auto opsTask = std::make_shared<OpsTask>(surface->renderTarget, surface->texture);
  tasks.push_back(opsTask);
  activeOpsTask = opsTask.get();
  return opsTask;
}

bool DrawingManager::flush(Semaphore* signalSemaphore) {
  auto* gpu = context->gpu();
  closeAllTasks();
  activeOpsTask = nullptr;
  for (auto& task : tasks) {
    task->execute(gpu);
  }
  removeAllTasks();
  return context->caps()->semaphoreSupport && gpu->insertSemaphore(signalSemaphore);
}

void DrawingManager::closeAllTasks() {
  for (auto& task : tasks) {
    task->makeClosed();
  }
}

void DrawingManager::removeAllTasks() {
  tasks.clear();
}
}  // namespace tgfx
