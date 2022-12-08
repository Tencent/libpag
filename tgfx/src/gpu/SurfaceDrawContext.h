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

#include "FragmentProcessor.h"
#include "gpu/ops/OpsTask.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/core/Rect.h"
#include "tgfx/gpu/Surface.h"

namespace tgfx {
class SurfaceDrawContext {
 public:
  explicit SurfaceDrawContext(Surface* surface) : surface(surface) {
  }

  void fillRectWithFP(const Rect& dstRect, const Matrix& localMatrix,
                      std::unique_ptr<FragmentProcessor> fp);

  void addOp(std::unique_ptr<Op> op);

 protected:
  OpsTask* getOpsTask();

  void replaceOpsTask();

  Surface* surface = nullptr;
  std::shared_ptr<OpsTask> opsTask;
};
}  // namespace tgfx
