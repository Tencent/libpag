/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include "tgfx/core/Image.h"
#include "tgfx/core/Rect.h"
#include "tgfx/layers/filters/LayerFilter.h"

namespace tgfx {
struct Point;
}

namespace pagx {

// Layer filter for the PAGX LevelsIndividualFilter node (spec 7.5). Reuses the legacy levels
// RuntimeFilter (src/rendering/filters/LevelsIndividualFilter.cpp) so both pipelines remap
// colors identically; it runs cache-less because the PAGX layer path has no pag::RenderCache.
// The twenty level values (global/red/green/blue input-black, input-white, gamma, output-black,
// output-white) live in 0-1 space and carry over raw. The filter never changes bounds.
class LevelsIndividualLayerFilter : public tgfx::LayerFilter {
 public:
  static std::shared_ptr<LevelsIndividualLayerFilter> Make(
      float inputBlack, float inputWhite, float gamma, float outputBlack, float outputWhite,
      float redInputBlack, float redInputWhite, float redGamma, float redOutputBlack,
      float redOutputWhite, float greenInputBlack, float greenInputWhite, float greenGamma,
      float greenOutputBlack, float greenOutputWhite, float blueInputBlack, float blueInputWhite,
      float blueGamma, float blueOutputBlack, float blueOutputWhite);
  float inputBlack() const {
    return _inputBlack;
  }

  float inputWhite() const {
    return _inputWhite;
  }

  float gamma() const {
    return _gamma;
  }

  float outputBlack() const {
    return _outputBlack;
  }

  float outputWhite() const {
    return _outputWhite;
  }

  float redInputBlack() const {
    return _redInputBlack;
  }

  float redInputWhite() const {
    return _redInputWhite;
  }

  float redGamma() const {
    return _redGamma;
  }

  float redOutputBlack() const {
    return _redOutputBlack;
  }

  float redOutputWhite() const {
    return _redOutputWhite;
  }

  float greenInputBlack() const {
    return _greenInputBlack;
  }

  float greenInputWhite() const {
    return _greenInputWhite;
  }

  float greenGamma() const {
    return _greenGamma;
  }

  float greenOutputBlack() const {
    return _greenOutputBlack;
  }

  float greenOutputWhite() const {
    return _greenOutputWhite;
  }

  float blueInputBlack() const {
    return _blueInputBlack;
  }

  float blueInputWhite() const {
    return _blueInputWhite;
  }

  float blueGamma() const {
    return _blueGamma;
  }

  float blueOutputBlack() const {
    return _blueOutputBlack;
  }

  float blueOutputWhite() const {
    return _blueOutputWhite;
  }

  void setInputBlack(float value);
  void setInputWhite(float value);
  void setGamma(float value);
  void setOutputBlack(float value);
  void setOutputWhite(float value);
  void setRedInputBlack(float value);
  void setRedInputWhite(float value);
  void setRedGamma(float value);
  void setRedOutputBlack(float value);
  void setRedOutputWhite(float value);
  void setGreenInputBlack(float value);
  void setGreenInputWhite(float value);
  void setGreenGamma(float value);
  void setGreenOutputBlack(float value);
  void setGreenOutputWhite(float value);
  void setBlueInputBlack(float value);
  void setBlueInputWhite(float value);
  void setBlueGamma(float value);
  void setBlueOutputBlack(float value);
  void setBlueOutputWhite(float value);

 protected:
  std::shared_ptr<tgfx::Image> onFilterImage(std::shared_ptr<tgfx::Image> input, float scale,
                                             const tgfx::Rect& contentBounds, tgfx::Point* offset,
                                             const tgfx::Point& inputOrigin) override;

 private:
  float _inputBlack = 0.0f;
  float _inputWhite = 1.0f;
  float _gamma = 1.0f;
  float _outputBlack = 0.0f;
  float _outputWhite = 1.0f;
  float _redInputBlack = 0.0f;
  float _redInputWhite = 1.0f;
  float _redGamma = 1.0f;
  float _redOutputBlack = 0.0f;
  float _redOutputWhite = 1.0f;
  float _greenInputBlack = 0.0f;
  float _greenInputWhite = 1.0f;
  float _greenGamma = 1.0f;
  float _greenOutputBlack = 0.0f;
  float _greenOutputWhite = 1.0f;
  float _blueInputBlack = 0.0f;
  float _blueInputWhite = 1.0f;
  float _blueGamma = 1.0f;
  float _blueOutputBlack = 0.0f;
  float _blueOutputWhite = 1.0f;
};

}  // namespace pagx
