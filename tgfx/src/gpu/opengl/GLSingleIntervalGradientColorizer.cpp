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

#include "GLSingleIntervalGradientColorizer.h"
#include "gpu/gradients/SingleIntervalGradientColorizer.h"

namespace pag {
void GLSingleIntervalGradientColorizer::emitCode(EmitArgs& args) {
  auto* fragBuilder = args.fragBuilder;
  std::string startName;
  startUniform = args.uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float4,
                                                 "start", &startName);
  std::string endName;
  endUniform = args.uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float4,
                                               "end", &endName);
  fragBuilder->codeAppendf("float t = %s.x;", args.inputColor.c_str());
  fragBuilder->codeAppendf("%s = (1.0 - t) * %s + t * %s;", args.outputColor.c_str(),
                           startName.c_str(), endName.c_str());
}

void GLSingleIntervalGradientColorizer::onSetData(const ProgramDataManager& programDataManager,
                                                  const FragmentProcessor& fragmentProcessor) {
  const auto& fp = static_cast<const SingleIntervalGradientColorizer&>(fragmentProcessor);
  if (startPrev != fp.start) {
    startPrev = fp.start;
    programDataManager.set4fv(startUniform, 1, fp.start.vector());
  }
  if (endPrev != fp.end) {
    endPrev = fp.end;
    programDataManager.set4fv(endUniform, 1, fp.end.vector());
  }
}
}  // namespace pag
