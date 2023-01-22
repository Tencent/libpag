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

#include <optional>
#include "GLBuffer.h"
#include "GLProgram.h"
#include "gpu/AAType.h"
#include "gpu/FragmentProcessor.h"
#include "gpu/GeometryProcessor.h"
#include "gpu/OpsRenderPass.h"
#include "gpu/Pipeline.h"
#include "gpu/ops/Op.h"
#include "tgfx/core/BlendMode.h"

namespace tgfx {
class GLOpsRenderPass : public OpsRenderPass {
 public:
  static std::unique_ptr<GLOpsRenderPass> Make(Context* context);

  void set(std::shared_ptr<RenderTarget> renderTarget,
           std::shared_ptr<Texture> renderTargetTexture);

  void reset();

  void bindPipelineAndScissorClip(const ProgramInfo& info, const Rect& drawBounds) override;

  void bindVertexBuffer(std::shared_ptr<GpuBuffer> vertexBuffer) override;

  void bindVerticesAndIndices(std::vector<float> vertices,
                              std::shared_ptr<GpuBuffer> indices) override;

  void draw(PrimitiveType primitiveType, int baseVertex, int vertexCount) override;

  void drawIndexed(PrimitiveType primitiveType, int baseIndex, int indexCount) override;

  void clear(const Rect& scissor, Color color) override;

 private:
  class Vertex : public Resource {
   public:
    bool init(Context* context);

    size_t memoryUsage() const override;

    unsigned array = 0;
    unsigned buffer = 0;

   protected:
    void computeRecycleKey(BytesKey*) const override;

   private:
    void onReleaseGPU() override;
  };

  explicit GLOpsRenderPass(Context* context, std::shared_ptr<Vertex> vertex)
      : OpsRenderPass(context), vertex(std::move(vertex)) {
  }

  void draw(const std::function<void()>& func);

  GLProgram* program = nullptr;
  std::vector<float> _vertices;
  std::shared_ptr<GLBuffer> _indexBuffer;
  std::shared_ptr<GLBuffer> _vertexBuffer;
  std::shared_ptr<Vertex> vertex;
};
}  // namespace tgfx
