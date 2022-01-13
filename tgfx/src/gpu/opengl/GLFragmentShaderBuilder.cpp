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

#include "GLFragmentShaderBuilder.h"
#include "GLContext.h"
#include "GLProgramBuilder.h"

namespace pag {
static constexpr char kDstColorName[] = "_dstColor";

GLFragmentShaderBuilder::GLFragmentShaderBuilder(ProgramBuilder* program)
    : FragmentShaderBuilder(program) {
  setPrecisionQualifier("precision mediump float;");
}

std::string GLFragmentShaderBuilder::dstColor() {
  const auto* gl = static_cast<GLProgramBuilder*>(programBuilder)->gl();
  if (gl->caps->frameBufferFetchSupport) {
    addFeature(PrivateFeature::FramebufferFetch, gl->caps->frameBufferFetchExtensionString);
    return gl->caps->frameBufferFetchColorName;
  }
  return kDstColorName;
}

std::string GLFragmentShaderBuilder::colorOutputName() {
  return static_cast<GLProgramBuilder*>(programBuilder)->isDesktopGL() ? CustomColorOutputName()
                                                                       : "gl_FragColor";
}
}  // namespace pag
