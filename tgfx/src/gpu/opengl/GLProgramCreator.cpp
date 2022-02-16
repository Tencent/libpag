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

#include "GLProgramCreator.h"
#include "GLContext.h"
#include "GLProgramBuilder.h"

namespace tgfx {
GLProgramCreator::GLProgramCreator(const GeometryProcessor* geometryProcessor,
                                   const Pipeline* pipeline)
    : geometryProcessor(geometryProcessor), pipeline(pipeline) {
}

void GLProgramCreator::computeUniqueKey(Context* context, BytesKey* bytesKey) const {
  geometryProcessor->computeProcessorKey(context, bytesKey);
  pipeline->computeKey(context, bytesKey);
}

std::unique_ptr<Program> GLProgramCreator::createProgram(Context* context) const {
  const auto* gl = GLContext::Unwrap(context);
  return GLProgramBuilder::CreateProgram(gl, geometryProcessor, pipeline);
}
}  // namespace tgfx
