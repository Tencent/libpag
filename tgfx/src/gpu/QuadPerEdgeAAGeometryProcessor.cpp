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

#include "QuadPerEdgeAAGeometryProcessor.h"
#include "core/utils/UniqueID.h"
#include "gpu/opengl/GLQuadPerEdgeAAGeometryProcessor.h"
#include "tgfx/gpu/YUVTexture.h"

namespace tgfx {
std::unique_ptr<QuadPerEdgeAAGeometryProcessor> QuadPerEdgeAAGeometryProcessor::Make(
    int width, int height, AAType aa, bool hasColor) {
  return std::unique_ptr<QuadPerEdgeAAGeometryProcessor>(
      new QuadPerEdgeAAGeometryProcessor(width, height, aa, hasColor));
}

QuadPerEdgeAAGeometryProcessor::QuadPerEdgeAAGeometryProcessor(int width, int height, AAType aa,
                                                               bool hasColor)
    : width(width), height(height), aa(aa) {
  if (aa == AAType::Coverage) {
    position = {"aPositionWithCoverage", ShaderVar::Type::Float3};
  } else {
    position = {"aPosition", ShaderVar::Type::Float2};
  }
  localCoord = {"localCoord", ShaderVar::Type::Float2};
  if (hasColor) {
    color = {"inColor", ShaderVar::Type::Float4};
  }
  setVertexAttributes(&position, 3);
}

void QuadPerEdgeAAGeometryProcessor::onComputeProcessorKey(BytesKey* bytesKey) const {
  static auto Type = UniqueID::Next();
  bytesKey->write(Type);
  uint32_t flags = aa == AAType::Coverage ? 1 : 0;
  flags |= color.isInitialized() ? 2 : 0;
  bytesKey->write(flags);
}

std::unique_ptr<GLGeometryProcessor> QuadPerEdgeAAGeometryProcessor::createGLInstance() const {
  return std::make_unique<GLQuadPerEdgeAAGeometryProcessor>();
}
}  // namespace tgfx
