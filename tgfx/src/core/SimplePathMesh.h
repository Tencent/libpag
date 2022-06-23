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

#include <optional>
#include "tgfx/core/Mesh.h"

namespace tgfx {
class SimplePathMesh : public Mesh {
 public:
  explicit SimplePathMesh(Rect rect) : rect(rect) {
  }

  explicit SimplePathMesh(RRect rRect) : rRect(rRect) {
  }

  Rect bounds() const override;

 private:
  std::unique_ptr<GLDrawOp> getOp(const Matrix& viewMatrix) const override;

  std::optional<Rect> rect;
  std::optional<RRect> rRect;
};
}  // namespace tgfx
