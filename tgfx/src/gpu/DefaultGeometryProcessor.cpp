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

#include "DefaultGeometryProcessor.h"
#include "core/utils/UniqueID.h"
#include "gpu/opengl/GLDefaultGeometryProcessor.h"

namespace tgfx {
std::unique_ptr<DefaultGeometryProcessor> DefaultGeometryProcessor::Make(
    int width, int height, const Matrix& localMatrix) {
  return std::unique_ptr<DefaultGeometryProcessor>(
      new DefaultGeometryProcessor(width, height, localMatrix));
}

DefaultGeometryProcessor::DefaultGeometryProcessor(int width, int height, const Matrix& localMatrix)
    : width(width), height(height), localMatrix(localMatrix) {
  position = {"aPosition", ShaderVar::Type::Float2};
  coverage = {"inCoverage", ShaderVar::Type::Float};
  setVertexAttributes(&position, 2);
}

void DefaultGeometryProcessor::onComputeProcessorKey(BytesKey* bytesKey) const {
  static auto Type = UniqueID::Next();
  bytesKey->write(Type);
}

std::unique_ptr<GLGeometryProcessor> DefaultGeometryProcessor::createGLInstance() const {
  return std::make_unique<GLDefaultGeometryProcessor>();
}
}  // namespace tgfx
