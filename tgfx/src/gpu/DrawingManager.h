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

#include <vector>
#include "OpsTask.h"
#include "RenderTask.h"
#include "tgfx/gpu/Surface.h"

namespace tgfx {
class DrawingManager {
 public:
  explicit DrawingManager(Context* context) : context(context) {
  }

  std::shared_ptr<OpsTask> newOpsTask(Surface* surface);

  bool flush(Surface* surface, Semaphore* signalSemaphore);

  void newTextureResolveRenderTask(Surface* surface);

 private:
  void closeAllTasks();

  void removeAllTasks();

  void closeActiveOpsTask();

  Context* context = nullptr;
  std::vector<std::shared_ptr<RenderTask>> tasks;
  OpsTask* activeOpsTask = nullptr;
};
}  // namespace tgfx
