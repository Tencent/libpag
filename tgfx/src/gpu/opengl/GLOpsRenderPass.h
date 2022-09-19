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
#include "gpu/Op.h"
#include "gpu/OpsRenderPass.h"
#include "gpu/Pipeline.h"
#include "tgfx/core/BlendMode.h"

namespace tgfx {
class GLDrawOp : public Op {
 public:
  explicit GLDrawOp(uint8_t classID) : Op(classID) {
  }

  ProgramInfo createProgram(OpsRenderPass* opsRenderPass, std::unique_ptr<GeometryProcessor> gp);

  const Rect& scissorRect() const {
    return _scissorRect;
  }

  void setScissorRect(Rect scissorRect) {
    _scissorRect = scissorRect;
  }

  void setBlendFactors(std::optional<std::pair<unsigned, unsigned>> blendFactors) {
    _blendFactors = blendFactors;
  }

  void setXferProcessor(std::unique_ptr<XferProcessor> xferProcessor) {
    _xferProcessor = std::move(xferProcessor);
  }

  void setRequireDstTexture(bool requiresDstTexture) {
    _requiresDstTexture = requiresDstTexture;
  }

  void setAA(AAType type) {
    aa = type;
  }

  void setColors(std::vector<std::unique_ptr<FragmentProcessor>> colors) {
    _colors = std::move(colors);
  }

  void setMasks(std::vector<std::unique_ptr<FragmentProcessor>> masks) {
    _masks = std::move(masks);
  }

 protected:
  bool onCombineIfPossible(Op* op) override;

  AAType aa = AAType::None;

 private:
  Rect _scissorRect = Rect::MakeEmpty();
  std::optional<std::pair<unsigned, unsigned>> _blendFactors;
  bool _requiresDstTexture = false;
  std::vector<std::unique_ptr<FragmentProcessor>> _colors;
  std::vector<std::unique_ptr<FragmentProcessor>> _masks;
  std::unique_ptr<XferProcessor> _xferProcessor;
};

class GLOpsRenderPass : public OpsRenderPass {
 public:
  static std::unique_ptr<GLOpsRenderPass> Make(Context* context);

  void set(std::shared_ptr<RenderTarget> renderTarget,
           std::shared_ptr<Texture> renderTargetTexture);

  void reset();

  void bindPipelineAndScissorClip(const ProgramInfo& info, const Rect& drawBounds) override;

  void bindVerticesAndIndices(std::vector<float> vertices,
                              std::shared_ptr<Resource> indices = nullptr) override;

  void draw(unsigned primitiveType, int baseVertex, int vertexCount) override;

  void drawIndexed(unsigned primitiveType, int baseIndex, int indexCount) override;

  void clear(const Rect& scissor, Color color) override;

 private:
  class Vertex : public Resource {
   public:
    bool init(Context* context);

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

  void draw(std::function<void()> func);

  GLProgram* program = nullptr;
  std::vector<float> _vertices;
  std::shared_ptr<GLBuffer> _indexBuffer;
  std::shared_ptr<Vertex> vertex;
};
}  // namespace tgfx
