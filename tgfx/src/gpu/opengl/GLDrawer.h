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
#include "gpu/Pipeline.h"
#include "tgfx/core/BlendMode.h"

namespace tgfx {
class GLDrawer;
struct DrawArgs {
  Context* context = nullptr;
  std::shared_ptr<RenderTarget> renderTarget = nullptr;
  std::shared_ptr<Texture> renderTargetTexture = nullptr;
  GLDrawer* drawer = nullptr;
};

struct ProgramInfo {
  Program* program = nullptr;
  std::unique_ptr<Pipeline> pipeline;
  std::unique_ptr<GeometryProcessor> geometryProcessor;
  std::optional<std::pair<unsigned, unsigned>> blendFactors;
};

#define DEFINE_OP_CLASS_ID                   \
  static uint8_t ClassID() {                 \
    static uint8_t ClassID = GenOpClassID(); \
    return ClassID;                          \
  }

class GLOp {
 public:
  explicit GLOp(uint8_t classID) : _classID(classID) {
  }

  virtual ~GLOp() = default;

  virtual void draw(const DrawArgs& args) = 0;

  bool combineIfPossible(GLOp* op);

  const Rect& bounds() const {
    return _bounds;
  }

 protected:
  static uint8_t GenOpClassID();

  void setBounds(Rect bounds) {
    _bounds = bounds;
  }

  void setTransformedBounds(const Rect& srcBounds, const Matrix& matrix) {
    _bounds = matrix.mapRect(srcBounds);
  }

  virtual bool onCombineIfPossible(GLOp*) {
    return false;
  }

 private:
  uint8_t _classID = 0;
  Rect _bounds = Rect::MakeEmpty();
};

class GLDrawOp : public GLOp {
 public:
  explicit GLDrawOp(uint8_t classID) : GLOp(classID) {
  }

  ProgramInfo createProgram(const DrawArgs& args, std::unique_ptr<GeometryProcessor> gp);

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
  bool onCombineIfPossible(GLOp* op) override;

  AAType aa = AAType::None;

 private:
  Rect _scissorRect = Rect::MakeEmpty();
  std::optional<std::pair<unsigned, unsigned>> _blendFactors;
  bool _requiresDstTexture = false;
  std::vector<std::unique_ptr<FragmentProcessor>> _colors;
  std::vector<std::unique_ptr<FragmentProcessor>> _masks;
  std::unique_ptr<XferProcessor> _xferProcessor;
};

class GLDrawer : public Resource {
 public:
  static std::shared_ptr<GLDrawer> Make(Context* context);

  void set(RenderTarget* rt) {
    _renderTarget = rt;
  }

  void reset() {
    program = nullptr;
    _vertices = {};
    _indexBuffer = nullptr;
    _renderTarget = nullptr;
  }

  void bindPipelineAndScissorClip(const ProgramInfo& info, const Rect& drawBounds);

  void bindVerticesAndIndices(std::vector<float> vertices,
                              std::shared_ptr<GLBuffer> indices = nullptr);

  void draw(unsigned primitiveType, int baseVertex, int vertexCount);

  void drawIndexed(unsigned primitiveType, int baseIndex, int indexCount);

  void clear(const Rect& scissor, Color color);

 protected:
  void computeRecycleKey(BytesKey*) const override;

 private:
  bool init(Context* context);

  void onReleaseGPU() override;

  void draw(std::function<void()> func);

  unsigned vertexArray = 0;
  unsigned vertexBuffer = 0;
  GLProgram* program = nullptr;
  std::vector<float> _vertices;
  std::shared_ptr<GLBuffer> _indexBuffer;
  RenderTarget* _renderTarget = nullptr;
};
}  // namespace tgfx
