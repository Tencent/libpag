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

#include "GLAlphaFragmentProcessor.h"
#include "gpu/AlphaFragmentProcessor.h"

namespace tgfx {
void GLAlphaFragmentProcessor::emitCode(EmitArgs& args) {
  auto* fragBuilder = args.fragBuilder;
  auto* uniformHandler = args.uniformHandler;

  std::string alphaUniformName;
  alphaUniform = uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float, "Alpha",
                                            &alphaUniformName);
  fragBuilder->codeAppendf("%s = %s * %s;", args.outputColor.c_str(), args.inputColor.c_str(),
                           alphaUniformName.c_str());
}

void GLAlphaFragmentProcessor::onSetData(const ProgramDataManager& programDataManager,
                                         const FragmentProcessor& fragmentProcessor) {
  const auto& afp = static_cast<const AlphaFragmentProcessor&>(fragmentProcessor);
  if (alphaPrev != afp.alpha) {
    alphaPrev = afp.alpha;
    programDataManager.set1f(alphaUniform, afp.alpha);
  }
}
}  // namespace tgfx
