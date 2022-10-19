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

#include "GeometryProcessor.h"
#include "Pipeline.h"
#include "Program.h"
#include "tgfx/core/Color.h"
#include "tgfx/gpu/Context.h"

namespace tgfx {
struct ProgramInfo {
  Program* program = nullptr;
  std::unique_ptr<Pipeline> pipeline;
  std::unique_ptr<GeometryProcessor> geometryProcessor;
  std::optional<std::pair<unsigned, unsigned>> blendFactors;
};

class OpsRenderPass {
 public:
  explicit OpsRenderPass(Context* context) : _context(context) {
  }

  Context* context() {
    return _context;
  }

  std::shared_ptr<RenderTarget> renderTarget() {
    return _renderTarget;
  }

  std::shared_ptr<Texture> renderTargetTexture() {
    return _renderTargetTexture;
  }

  virtual ~OpsRenderPass() = default;

  virtual void bindPipelineAndScissorClip(const ProgramInfo& info, const Rect& drawBounds) = 0;

  virtual void bindVertexBuffer(std::shared_ptr<GpuBuffer> vertexBuffer) = 0;

  virtual void bindVerticesAndIndices(std::vector<float> vertices,
                                      std::shared_ptr<GpuBuffer> indices) = 0;

  virtual void draw(unsigned primitiveType, int baseVertex, int vertexCount) = 0;

  virtual void drawIndexed(unsigned primitiveType, int baseIndex, int indexCount) = 0;

  virtual void clear(const Rect& scissor, Color color) = 0;

 protected:
  Context* _context = nullptr;
  std::shared_ptr<RenderTarget> _renderTarget;
  std::shared_ptr<Texture> _renderTargetTexture;
};
}  // namespace tgfx
