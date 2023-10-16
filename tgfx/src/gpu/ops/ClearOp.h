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

#include "Op.h"

namespace tgfx {
class ClearOp : public Op {
 public:
  static std::unique_ptr<ClearOp> Make(Color color, const Rect& scissor);

  void execute(RenderPass* renderPass) override;

 private:
  DEFINE_OP_CLASS_ID

  explicit ClearOp(Color color, const Rect& scissor)
      : Op(ClassID()), color(color), scissor(scissor) {
  }

  bool onCombineIfPossible(Op* op) override;

  Color color = Color::Transparent();
  Rect scissor = Rect::MakeEmpty();
};
}  // namespace tgfx
