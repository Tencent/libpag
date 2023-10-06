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

#include "GLGeometryProcessor.h"

namespace tgfx {
static constexpr char TRANSFORM_UNIFORM_PREFIX[] = "CoordTransformMatrix_";

void GLGeometryProcessor::setTransformDataHelper(const Matrix& localMatrix,
                                                 UniformBuffer* uniformBuffer,
                                                 FPCoordTransformIter* transformIter) {
  int i = 0;
  while (const CoordTransform* coordTransform = transformIter->next()) {
    Matrix combined = Matrix::I();
    combined.setConcat(coordTransform->getTotalMatrix(), localMatrix);
    std::string uniformName = TRANSFORM_UNIFORM_PREFIX;
    uniformName += std::to_string(i);
    uniformBuffer->setMatrix(uniformName, combined);
    ++i;
  }
}

void GLGeometryProcessor::emitTransforms(VertexShaderBuilder* vertexBuilder,
                                         VaryingHandler* varyingHandler,
                                         UniformHandler* uniformHandler,
                                         const ShaderVar& localCoordsVar,
                                         FPCoordTransformHandler* transformHandler) {
  std::string localCoords = "vec3(";
  localCoords += localCoordsVar.name();
  localCoords += ", 1)";
  int i = 0;
  while (transformHandler->nextCoordTransform() != nullptr) {
    std::string strUniName = TRANSFORM_UNIFORM_PREFIX;
    strUniName += std::to_string(i);
    std::string uniName;
    uniformHandler->addUniform(ShaderFlags::Vertex, ShaderVar::Type::Float3x3, strUniName,
                               &uniName);
    std::string strVaryingName = "TransformedCoords_";
    strVaryingName += std::to_string(i);
    ShaderVar::Type varyingType = ShaderVar::Type::Float2;
    auto varying = varyingHandler->addVarying(strVaryingName, varyingType);

    transformHandler->specifyCoordsForCurrCoordTransform(varying.name(), varyingType);

    vertexBuilder->codeAppendf("%s = (%s * %s).xy;", varying.vsOut().c_str(), uniName.c_str(),
                               localCoords.c_str());
    ++i;
  }
}

}  // namespace tgfx
