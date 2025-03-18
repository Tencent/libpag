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

namespace pag {
class BulgeUniforms : public Uniforms {
 public:
  BulgeUniforms(tgfx::Context* context, unsigned program);

  int horizontalRadiusHandle = -1;
  int verticalRadiusHandle = -1;
  int bulgeCenterHandle = -1;
  int bulgeHeightHandle = -1;
  int pinningHandle = -1;
};

class BulgeRuntimeFilter : public RuntimeFilter {
 public:
  DEFINE_RUNTIME_EFFECT_TYPE
  BulgeRuntimeFilter(float horizontalRadius, float verticalRadius, const Point& bulgeCenter,
                     float bulgeHeight, float pinning);

 protected:
  std::string onBuildVertexShader() const override;

  std::string onBuildFragmentShader() const override;

  std::unique_ptr<Uniforms> onPrepareProgram(tgfx::Context* context,
                                             unsigned program) const override;

  void onUpdateParams(tgfx::Context* context, const RuntimeProgram* program,
                      const std::vector<tgfx::BackendTexture>& sources) const override;

  std::vector<float> computeVertices(const std::vector<tgfx::BackendTexture>& sources,
                                     const tgfx::BackendRenderTarget& target,
                                     const tgfx::Point& offset) const override;

  tgfx::Rect filterBounds(const tgfx::Rect& srcRect) const override;

 private:
  float horizontalRadius = 0.f;
  float verticalRadius = 0.f;
  Point bulgeCenter = {};
  float bulgeHeight = 0.f;
  float pinning = 0.f;
};

class BulgeFilter : public EffectFilter {
 public:
  explicit BulgeFilter(Effect* effect);
  ~BulgeFilter() override = default;

  void update(Frame layerFrame, const tgfx::Point& sourceScale) override;

 protected:
  std::shared_ptr<tgfx::RuntimeEffect> createRuntimeEffect() override;

 private:
  Effect* effect = nullptr;
  float horizontalRadius = 0.f;
  float verticalRadius = 0.f;
  Point bulgeCenter = {};
  float bulgeHeight = 0.f;
  float pinning = 0.f;
};
}  // namespace pag
