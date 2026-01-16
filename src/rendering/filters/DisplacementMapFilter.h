/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "RuntimeFilter.h"
#include "pag/file.h"
#include "tgfx/core/Size.h"

namespace pag {
class RenderCache;

class DisplacementMapFilter : public RuntimeFilter {
 public:
  static std::shared_ptr<tgfx::Image> Apply(std::shared_ptr<tgfx::Image> input, Effect* effect,
                                            Layer* layer, RenderCache* cache,
                                            const tgfx::Matrix& layerMatrix, Frame layerFrame,
                                            const tgfx::Rect& contentBounds, tgfx::Point* offset);

  DisplacementMapFilter(RenderCache* cache, DisplacementMapSource useForHorizontalDisplacement,
                        float maxHorizontalDisplacement,
                        DisplacementMapSource useForVerticalDisplacement,
                        float maxVerticalDisplacement,
                        DisplacementMapBehavior displacementMapBehavior, bool edgeBehavior,
                        bool expandOutput, float effectOpacity, tgfx::Matrix layerMatrix,
                        tgfx::Size size, tgfx::Size displacementSize, tgfx::Rect contentBounds,
                        std::shared_ptr<tgfx::Image> sourceImage)
      : RuntimeFilter(cache, {std::move(sourceImage)}),
        useForHorizontalDisplacement(useForHorizontalDisplacement),
        maxHorizontalDisplacement(maxHorizontalDisplacement),
        useForVerticalDisplacement(useForVerticalDisplacement),
        maxVerticalDisplacement(maxVerticalDisplacement),
        displacementMapBehavior(displacementMapBehavior), edgeBehavior(edgeBehavior),
        expandOutput(expandOutput), effectOpacity(effectOpacity), layerMatrix(layerMatrix),
        size(size), displacementSize(displacementSize), contentBounds(contentBounds) {
  }

 protected:
  DEFINE_RUNTIME_FILTER_TYPE

  std::string onBuildFragmentShader() const override;

  std::vector<tgfx::BindingEntry> uniformBlocks() const override;

  std::vector<tgfx::BindingEntry> textureSamplers() const override;

  void onUpdateUniforms(tgfx::RenderPass* renderPass, tgfx::GPU* gpu,
                        const std::vector<std::shared_ptr<tgfx::Texture>>& inputTextures,
                        const tgfx::Point& offset) const override;

  tgfx::Rect filterBounds(const tgfx::Rect& srcRect) const override;

  DisplacementMapSource useForHorizontalDisplacement = DisplacementMapSource::Red;
  float maxHorizontalDisplacement = 0.f;
  DisplacementMapSource useForVerticalDisplacement = DisplacementMapSource::Red;
  float maxVerticalDisplacement = 0.f;
  DisplacementMapBehavior displacementMapBehavior = DisplacementMapBehavior::CenterMap;
  bool edgeBehavior = false;
  bool expandOutput = true;
  float effectOpacity = 1.f;
  tgfx::Matrix layerMatrix = tgfx::Matrix::I();
  tgfx::Size size = {0, 0};
  tgfx::Size displacementSize = {0, 0};
  tgfx::Rect contentBounds = {0, 0, 0, 0};
};

}  // namespace pag
