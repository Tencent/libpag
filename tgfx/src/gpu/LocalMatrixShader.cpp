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

#include "LocalMatrixShader.h"
#include "gpu/FragmentProcessor.h"

namespace tgfx {
std::shared_ptr<Shader> Shader::makeWithLocalMatrix(const Matrix& matrix) const {
  if (matrix.isIdentity()) {
    return weakThis.lock();
  }
  Matrix localMatrix = Matrix::I();
  auto baseShader = makeAsALocalMatrixShader(&localMatrix);
  if (baseShader) {
    localMatrix.preConcat(matrix);
  } else {
    localMatrix = matrix;
    baseShader = weakThis.lock();
    if (baseShader == nullptr) {
      return nullptr;
    }
  }
  auto shader = std::make_shared<LocalMatrixShader>(std::move(baseShader), localMatrix);
  shader->weakThis = shader;
  return shader;
}

std::shared_ptr<Shader> LocalMatrixShader::makeAsALocalMatrixShader(Matrix* localMatrix) const {
  if (localMatrix) {
    *localMatrix = _localMatrix;
  }
  return proxyShader;
}

std::unique_ptr<FragmentProcessor> LocalMatrixShader::asFragmentProcessor(
    const FPArgs& args) const {
  auto matrix = _localMatrix;
  matrix.preConcat(args.localMatrix);
  FPArgs fpArgs(args.context, matrix);
  return proxyShader->asFragmentProcessor(fpArgs);
}
}  // namespace tgfx
