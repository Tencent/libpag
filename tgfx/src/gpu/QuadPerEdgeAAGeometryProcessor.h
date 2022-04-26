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

#pragma once

#include "AAType.h"
#include "GeometryProcessor.h"
#include "tgfx/gpu/Paint.h"

namespace tgfx {
class QuadPerEdgeAAGeometryProcessor : public GeometryProcessor {
 public:
  static std::unique_ptr<QuadPerEdgeAAGeometryProcessor> Make(int width, int height, AAType aa,
                                                              bool hasColor);

  std::string name() const override {
    return "QuadPerEdgeAAGeometryProcessor";
  }

  std::unique_ptr<GLGeometryProcessor> createGLInstance() const override;

 private:
  QuadPerEdgeAAGeometryProcessor(int width, int height, AAType aa, bool hasColor);

  void onComputeProcessorKey(BytesKey* bytesKey) const override;

  Attribute position;  // May contain coverage as last channel
  Attribute localCoord;
  Attribute color;

  int width = 1;
  int height = 1;
  AAType aa = AAType::None;

  friend class GLQuadPerEdgeAAGeometryProcessor;
};
}  // namespace tgfx
