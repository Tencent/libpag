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
#include "gpu/YUVTexture.h"

namespace tgfx {
QuadPerEdgeAAGeometryProcessor::QuadPerEdgeAAGeometryProcessor(int width, int height, AAType aa,
                                                               bool hasColor)
    : GeometryProcessor(ClassID()), width(width), height(height), aa(aa) {
  if (aa == AAType::Coverage) {
    position = {"aPositionWithCoverage", SLType::Float3};
  } else {
    position = {"aPosition", SLType::Float2};
  }
  localCoord = {"localCoord", SLType::Float2};
  if (hasColor) {
    color = {"inColor", SLType::Float4};
  }
  setVertexAttributes(&position, 3);
}

void QuadPerEdgeAAGeometryProcessor::onComputeProcessorKey(BytesKey* bytesKey) const {
  uint32_t flags = aa == AAType::Coverage ? 1 : 0;
  flags |= color.isInitialized() ? 2 : 0;
  bytesKey->write(flags);
}
}  // namespace tgfx
