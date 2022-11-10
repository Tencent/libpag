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
#include "Op.h"
#include "OpsRenderPass.h"

namespace tgfx {
class DrawOp : public Op {
 public:
  explicit DrawOp(uint8_t classID) : Op(classID) {
  }

  ProgramInfo createProgram(OpsRenderPass* opsRenderPass, std::unique_ptr<GeometryProcessor> gp);

  const Rect& scissorRect() const {
    return _scissorRect;
  }

  void setScissorRect(Rect scissorRect) {
    _scissorRect = scissorRect;
  }

  void setBlendFactors(std::optional<std::pair<BlendModeCoeff, BlendModeCoeff>> blendFactors) {
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
  std::optional<std::pair<BlendModeCoeff, BlendModeCoeff>> _blendFactors;
  bool _requiresDstTexture = false;
  std::vector<std::unique_ptr<FragmentProcessor>> _colors;
  std::vector<std::unique_ptr<FragmentProcessor>> _masks;
  std::unique_ptr<XferProcessor> _xferProcessor;
};
}  // namespace tgfx
