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
struct DrawArgs {
  Context* context = nullptr;
  std::shared_ptr<RenderTarget> renderTarget = nullptr;
  std::shared_ptr<Texture> renderTargetTexture = nullptr;
};

struct ProgramInfo {
  GLProgram* program;
  std::unique_ptr<Pipeline> pipeline;
  std::unique_ptr<GeometryProcessor> geometryProcessor;
  std::optional<std::pair<unsigned, unsigned>> blendFactors;
};

#define DEFINE_OP_CLASS_ID                   \
  static uint8_t ClassID() {                 \
    static uint8_t ClassID = GenOpClassID(); \
    return ClassID;                          \
  }

class GLDrawOp {
 public:
  explicit GLDrawOp(uint8_t classID) : _classID(classID) {
  }

  virtual ~GLDrawOp() = default;

  virtual std::vector<float> vertices(const DrawArgs& args) = 0;

  virtual void draw(const DrawArgs& args) = 0;

  ProgramInfo createProgram(const DrawArgs& args);

  const Rect& bounds() const {
    return _bounds;
  }

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

  bool requiresDstTexture() const {
    return _requiresDstTexture;
  }

  bool combineIfPossible(GLDrawOp* that);

 protected:
  static uint8_t GenOpClassID();

  uint8_t classID() const {
    return _classID;
  }

  virtual bool onCombineIfPossible(GLDrawOp*) {
    return false;
  }

  virtual std::unique_ptr<GeometryProcessor> getGeometryProcessor(const DrawArgs& args) = 0;

  void setBounds(Rect bounds) {
    _bounds = bounds;
  }

  void setTransformedBounds(const Rect& srcBounds, const Matrix& matrix) {
    _bounds = matrix.mapRect(srcBounds);
  }

  AAType aa = AAType::None;

 private:
  uint8_t _classID = 0;
  Rect _bounds = Rect::MakeEmpty();
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

  void draw(const DrawArgs& args, std::unique_ptr<GLDrawOp> op) const;

  static void DrawIndexBuffer(Context* context, const std::shared_ptr<GLBuffer>& indexBuffer);

  static void DrawArrays(Context* context, unsigned mode, int first, int count);

 protected:
  void computeRecycleKey(BytesKey*) const override;

 private:
  bool init(Context* context);

  void onReleaseGPU() override;

  unsigned vertexArray = 0;
  unsigned vertexBuffer = 0;
};
}  // namespace tgfx
