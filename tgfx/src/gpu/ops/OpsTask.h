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

#include "gpu/RenderTask.h"
#include "gpu/ops/Op.h"
#include "tgfx/gpu/Surface.h"

namespace tgfx {
class OpsTask : public RenderTask {
 public:
  OpsTask(std::shared_ptr<RenderTarget> renderTarget, std::shared_ptr<Texture> texture)
      : RenderTask(std::move(renderTarget)), renderTargetTexture(std::move(texture)) {
  }

  ~OpsTask() override;

  void addOp(std::unique_ptr<Op> op);

  bool execute(Gpu* gpu) override;

 private:
  std::shared_ptr<Texture> renderTargetTexture = nullptr;
  std::vector<std::unique_ptr<Op>> ops;
};
}  // namespace tgfx
