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

#include "tgfx/core/Matrix.h"
#include "tgfx/gpu/Shader.h"

namespace tgfx {
class LocalMatrixShader final : public Shader {
 public:
  LocalMatrixShader(std::shared_ptr<Shader> proxy, const Matrix& localMatrix)
      : proxyShader(std::move(proxy)), _localMatrix(localMatrix) {
  }

  std::shared_ptr<Shader> makeAsALocalMatrixShader(Matrix* localMatrix) const override;

  std::unique_ptr<FragmentProcessor> asFragmentProcessor(const FPArgs& args) const override;

 private:
  std::shared_ptr<Shader> proxyShader;
  const Matrix _localMatrix;
};
}  // namespace tgfx
