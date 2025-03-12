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

#include "EffectFilter.h"
#include "RuntimeFilter.h"
#include "base/utils/TGFXCast.h"
#include "codec/tags/effects/CornerPinEffect.h"

namespace pag {
class CornerPinRuntimeFilter : public RuntimeFilter {
 public:
  DEFINE_RUNTIME_EFFECT_TYPE;
  explicit CornerPinRuntimeFilter(const Point cornerPoints[4]) : RuntimeFilter(Type()) {
    for (int i = 0; i < 4; i++) {
      this->cornerPoints[i] = ToTGFX(cornerPoints[i]);
    }
    calculateVertexQs();
  }

 protected:
  std::string onBuildVertexShader() const override;

  std::string onBuildFragmentShader() const override;

  std::vector<float> computeVertices(const std::vector<tgfx::BackendTexture>& sources,
                                     const tgfx::BackendRenderTarget& target,
                                     const tgfx::Point& offset) const override;

  void bindVertices(tgfx::Context* context, const RuntimeProgram* program,
                    const std::vector<float>& points) const override;

  int sampleCount() const override;

  tgfx::Rect filterBounds(const tgfx::Rect& srcRect) const override;

 private:
  void calculateVertexQs();
  tgfx::Point cornerPoints[4] = {};
  float vertexQs[4] = {};
};

class CornerPinFilter : public EffectFilter {
 public:
  explicit CornerPinFilter(Effect* effect);

  void update(Frame layerFrame, const tgfx::Point& sourceScale) override;

 protected:
  std::shared_ptr<tgfx::RuntimeEffect> createRuntimeEffect() override;

 private:
  Effect* effect = nullptr;
  Point points[4] = {};
};
}  // namespace pag
