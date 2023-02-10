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

#include "OpsTask.h"
#include "gpu/Gpu.h"
#include "gpu/OpsRenderPass.h"

namespace tgfx {
void OpsTask::addOp(std::unique_ptr<Op> op) {
  if (!ops.empty() && ops.back()->combineIfPossible(op.get())) {
    return;
  }
  ops.emplace_back(std::move(op));
}

bool OpsTask::execute(Gpu* gpu) {
  if (ops.empty()) {
    return false;
  }
  auto opsRenderPass = gpu->getOpsRenderPass(renderTarget, renderTargetTexture);
  if (opsRenderPass == nullptr) {
    return false;
  }
  std::for_each(ops.begin(), ops.end(), [gpu](auto& op) { op->prepare(gpu); });
  opsRenderPass->begin();
  auto tempOps = std::move(ops);
  for (auto& op : tempOps) {
    op->execute(opsRenderPass);
  }
  if (renderTargetTexture) {
    gpu->regenerateMipMapLevels(renderTargetTexture->getSampler());
  }
  opsRenderPass->end();
  gpu->submit(opsRenderPass);
  return true;
}

void OpsTask::gatherProxies(std::vector<TextureProxy*>* proxies) const {
  if (ops.empty()) {
    return;
  }
  auto func = [proxies](TextureProxy* proxy) { proxies->emplace_back(proxy); };
  std::for_each(ops.begin(), ops.end(), [&func](auto& op) { op->visitProxies(func); });
}
}  // namespace tgfx
