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

#include "GLColorMatrixFragmentProcessor.h"
#include "gpu/ColorMatrixFragmentProcessor.h"

namespace tgfx {
void GLColorMatrixFragmentProcessor::emitCode(EmitArgs& args) {
  auto* uniformHandler = args.uniformHandler;
  std::string matrixUniformName;
  matrixUniform = uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float4x4,
                                             "Matrix", &matrixUniformName);
  std::string vectorUniformName;
  vectorUniform = uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float4,
                                             "Vector", &vectorUniformName);

  auto* fragBuilder = args.fragBuilder;
  fragBuilder->codeAppendf("%s = vec4(%s.rgb / max(%s.a, 9.9999997473787516e-05), %s.a);",
                           args.outputColor.c_str(), args.inputColor.c_str(),
                           args.inputColor.c_str(), args.inputColor.c_str());
  fragBuilder->codeAppendf("%s = %s * %s + %s;", args.outputColor.c_str(),
                           matrixUniformName.c_str(), args.outputColor.c_str(),
                           vectorUniformName.c_str());
  fragBuilder->codeAppendf("%s = clamp(%s, 0.0, 1.0);", args.outputColor.c_str(),
                           args.outputColor.c_str());
  fragBuilder->codeAppendf("%s.rgb *= %s.a;", args.outputColor.c_str(), args.outputColor.c_str());
}

void GLColorMatrixFragmentProcessor::onSetData(const ProgramDataManager& programDataManager,
                                               const FragmentProcessor& fragmentProcessor) {
  const auto& fp = static_cast<const ColorMatrixFragmentProcessor&>(fragmentProcessor);
  if (matrixPrev != fp.matrix) {
    matrixPrev = fp.matrix;
    float matrix[] = {
        fp.matrix[0],  fp.matrix[5],  fp.matrix[10], fp.matrix[15], fp.matrix[1],  fp.matrix[6],
        fp.matrix[11], fp.matrix[16], fp.matrix[2],  fp.matrix[7],  fp.matrix[12], fp.matrix[17],
        fp.matrix[3],  fp.matrix[8],  fp.matrix[13], fp.matrix[18],
    };
    float vec[] = {
        fp.matrix[4],
        fp.matrix[9],
        fp.matrix[14],
        fp.matrix[19],
    };
    programDataManager.setMatrix4f(matrixUniform, matrix);
    programDataManager.set4fv(vectorUniform, 1, vec);
  }
}
}  // namespace tgfx
