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

#include "GLConstColorProcessor.h"

namespace tgfx {
std::unique_ptr<ConstColorProcessor> ConstColorProcessor::Make(Color color, InputMode mode) {
  return std::unique_ptr<ConstColorProcessor>(new GLConstColorProcessor(color, mode));
}

GLConstColorProcessor::GLConstColorProcessor(Color color, InputMode mode)
    : ConstColorProcessor(color, mode) {
}

void GLConstColorProcessor::emitCode(EmitArgs& args) const {
  auto* fragBuilder = args.fragBuilder;
  auto colorName =
      args.uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float4, "Color");
  fragBuilder->codeAppendf("%s = %s;", args.outputColor.c_str(), colorName.c_str());
  switch (inputMode) {
    case InputMode::Ignore:
      break;
    case InputMode::ModulateRGBA:
      fragBuilder->codeAppendf("%s *= %s;", args.outputColor.c_str(), args.inputColor.c_str());
      break;
    case InputMode::ModulateA:
      fragBuilder->codeAppendf("%s *= %s.a;", args.outputColor.c_str(), args.inputColor.c_str());
      break;
  }
}

void GLConstColorProcessor::onSetData(UniformBuffer* uniformBuffer) const {
  uniformBuffer->setData("Color", color.array());
}
}  // namespace tgfx
