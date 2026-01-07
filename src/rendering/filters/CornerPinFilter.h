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
#include "base/utils/TGFXCast.h"
#include "codec/tags/effects/CornerPinEffect.h"

namespace pag {
class CornerPinFilter : public RuntimeFilter {
 public:
  static std::shared_ptr<tgfx::Image> Apply(std::shared_ptr<tgfx::Image> input, Effect* effect,
                                            Frame layerFrame, const tgfx::Point& sourceScale,
                                            tgfx::Point* offset);

  explicit CornerPinFilter(const Point cornerPoints[4]) {
    for (int i = 0; i < 4; i++) {
      this->cornerPoints[i] = ToTGFX(cornerPoints[i]);
    }
    calculateVertexQs();
  }

 protected:
  std::string onBuildVertexShader() const override;

  std::string onBuildFragmentShader() const override;

  std::vector<tgfx::Attribute> vertexAttributes() const override;

  std::vector<float> computeVertices(const tgfx::Texture* source, const tgfx::Texture* target,
                                     const tgfx::Point& offset) const override;

  int sampleCount() const override;

  tgfx::Rect filterBounds(const tgfx::Rect& srcRect) const override;

 private:
  void calculateVertexQs();
  tgfx::Point cornerPoints[4] = {};
  float vertexQs[4] = {};
};

}  // namespace pag
