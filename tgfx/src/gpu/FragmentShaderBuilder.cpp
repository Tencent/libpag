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

#include "FragmentShaderBuilder.h"
#include "ProgramBuilder.h"

namespace pag {
FragmentShaderBuilder::FragmentShaderBuilder(ProgramBuilder* program) : ShaderBuilder(program) {
  subStageIndices.push_back(0);
}

void FragmentShaderBuilder::onFinalize() {
  programBuilder->varyingHandler()->getFragDecls(&shaderStrings[Type::Inputs]);
}

void FragmentShaderBuilder::declareCustomOutputColor() {
  outputs.emplace_back(CustomColorOutputName(), ShaderVar::Type::Float4,
                       ShaderVar::TypeModifier::Out);
}

void FragmentShaderBuilder::onBeforeChildProcEmitCode() {
  subStageIndices.push_back(0);
  // second-to-last value in the subStageIndices stack is the index of the child proc
  // at that level which is currently emitting code.
  _mangleString.append("_c");
  _mangleString += std::to_string(subStageIndices[subStageIndices.size() - 2]);
}

void FragmentShaderBuilder::onAfterChildProcEmitCode() {
  subStageIndices.pop_back();
  subStageIndices.back()++;
  auto removeAt = _mangleString.rfind('_');
  _mangleString.erase(removeAt, _mangleString.size() - removeAt);
}
}  // namespace pag
