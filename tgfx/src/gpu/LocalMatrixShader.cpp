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
std::shared_ptr<Shader> Shader::makeWithLocalMatrix(const Matrix& matrix, bool isPre) const {
  if (matrix.isIdentity()) {
    return weakThis.lock();
  }
  auto baseShader = weakThis.lock();
  if (baseShader == nullptr) {
    return nullptr;
  }
  std::shared_ptr<Shader> shader;
  if (isPre) {
    shader = std::make_shared<LocalMatrixShader>(std::move(baseShader), matrix, Matrix::I());
  } else {
    shader = std::make_shared<LocalMatrixShader>(std::move(baseShader), Matrix::I(), matrix);
  }
  shader->weakThis = shader;
  return shader;
}

std::shared_ptr<Shader> LocalMatrixShader::makeWithLocalMatrix(const Matrix& matrix,
                                                               bool isPre) const {
  if (matrix.isIdentity()) {
    return weakThis.lock();
  }
  std::shared_ptr<LocalMatrixShader> shader;
  if (isPre) {
    auto localMatrix = _preLocalMatrix;
    localMatrix.preConcat(matrix);
    shader = std::make_shared<LocalMatrixShader>(proxyShader, localMatrix, _postLocalMatrix);
  } else {
    auto localMatrix = _postLocalMatrix;
    localMatrix.postConcat(matrix);
    shader = std::make_shared<LocalMatrixShader>(proxyShader, _preLocalMatrix, localMatrix);
  }
  shader->weakThis = shader;
  return shader;
}

std::unique_ptr<FragmentProcessor> LocalMatrixShader::asFragmentProcessor(
    const FPArgs& args) const {
  FPArgs fpArgs(args);
  fpArgs.preLocalMatrix.setConcat(_preLocalMatrix, fpArgs.preLocalMatrix);
  fpArgs.postLocalMatrix.setConcat(fpArgs.postLocalMatrix, _postLocalMatrix);
  return proxyShader->asFragmentProcessor(fpArgs);
}
}  // namespace tgfx
