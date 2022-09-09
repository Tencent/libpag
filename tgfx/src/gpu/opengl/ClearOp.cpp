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

#include "ClearOp.h"

namespace tgfx {
std::unique_ptr<ClearOp> ClearOp::Make(Color color, const Rect& scissor) {
  return std::unique_ptr<ClearOp>(new ClearOp(color, scissor));
}

bool ContainsScissor(const Rect& a, const Rect& b) {
  return a.isEmpty() || (!b.isEmpty() && a.contains(b));
}

bool ClearOp::onCombineIfPossible(Op* op) {
  auto that = static_cast<ClearOp*>(op);
  if (ContainsScissor(that->scissor, scissor)) {
    scissor = that->scissor;
    color = that->color;
    return true;
  } else if (color == that->color && ContainsScissor(scissor, that->scissor)) {
    return true;
  }
  return false;
}

void ClearOp::execute(OpsRenderPass* opsRenderPass) {
  opsRenderPass->clear(scissor, color);
}
}  // namespace tgfx
