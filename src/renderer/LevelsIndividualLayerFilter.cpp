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

#include "LevelsIndividualLayerFilter.h"
#include <utility>
#include "rendering/filters/LevelsIndividualFilter.h"
#include "tgfx/core/ImageFilter.h"
#include "tgfx/core/Point.h"

namespace pagx {

std::shared_ptr<LevelsIndividualLayerFilter> LevelsIndividualLayerFilter::Make(
    float inputBlack, float inputWhite, float gamma, float outputBlack, float outputWhite,
    float redInputBlack, float redInputWhite, float redGamma, float redOutputBlack,
    float redOutputWhite, float greenInputBlack, float greenInputWhite, float greenGamma,
    float greenOutputBlack, float greenOutputWhite, float blueInputBlack, float blueInputWhite,
    float blueGamma, float blueOutputBlack, float blueOutputWhite) {
  auto filter = std::shared_ptr<LevelsIndividualLayerFilter>(new LevelsIndividualLayerFilter());
  filter->_inputBlack = inputBlack;
  filter->_inputWhite = inputWhite;
  filter->_gamma = gamma;
  filter->_outputBlack = outputBlack;
  filter->_outputWhite = outputWhite;
  filter->_redInputBlack = redInputBlack;
  filter->_redInputWhite = redInputWhite;
  filter->_redGamma = redGamma;
  filter->_redOutputBlack = redOutputBlack;
  filter->_redOutputWhite = redOutputWhite;
  filter->_greenInputBlack = greenInputBlack;
  filter->_greenInputWhite = greenInputWhite;
  filter->_greenGamma = greenGamma;
  filter->_greenOutputBlack = greenOutputBlack;
  filter->_greenOutputWhite = greenOutputWhite;
  filter->_blueInputBlack = blueInputBlack;
  filter->_blueInputWhite = blueInputWhite;
  filter->_blueGamma = blueGamma;
  filter->_blueOutputBlack = blueOutputBlack;
  filter->_blueOutputWhite = blueOutputWhite;
  return filter;
}

void LevelsIndividualLayerFilter::setInputBlack(float value) {
  if (_inputBlack == value) {
    return;
  }
  _inputBlack = value;
  invalidateFilter();
}

void LevelsIndividualLayerFilter::setInputWhite(float value) {
  if (_inputWhite == value) {
    return;
  }
  _inputWhite = value;
  invalidateFilter();
}

void LevelsIndividualLayerFilter::setGamma(float value) {
  if (_gamma == value) {
    return;
  }
  _gamma = value;
  invalidateFilter();
}

void LevelsIndividualLayerFilter::setOutputBlack(float value) {
  if (_outputBlack == value) {
    return;
  }
  _outputBlack = value;
  invalidateFilter();
}

void LevelsIndividualLayerFilter::setOutputWhite(float value) {
  if (_outputWhite == value) {
    return;
  }
  _outputWhite = value;
  invalidateFilter();
}

void LevelsIndividualLayerFilter::setRedInputBlack(float value) {
  if (_redInputBlack == value) {
    return;
  }
  _redInputBlack = value;
  invalidateFilter();
}

void LevelsIndividualLayerFilter::setRedInputWhite(float value) {
  if (_redInputWhite == value) {
    return;
  }
  _redInputWhite = value;
  invalidateFilter();
}

void LevelsIndividualLayerFilter::setRedGamma(float value) {
  if (_redGamma == value) {
    return;
  }
  _redGamma = value;
  invalidateFilter();
}

void LevelsIndividualLayerFilter::setRedOutputBlack(float value) {
  if (_redOutputBlack == value) {
    return;
  }
  _redOutputBlack = value;
  invalidateFilter();
}

void LevelsIndividualLayerFilter::setRedOutputWhite(float value) {
  if (_redOutputWhite == value) {
    return;
  }
  _redOutputWhite = value;
  invalidateFilter();
}

void LevelsIndividualLayerFilter::setGreenInputBlack(float value) {
  if (_greenInputBlack == value) {
    return;
  }
  _greenInputBlack = value;
  invalidateFilter();
}

void LevelsIndividualLayerFilter::setGreenInputWhite(float value) {
  if (_greenInputWhite == value) {
    return;
  }
  _greenInputWhite = value;
  invalidateFilter();
}

void LevelsIndividualLayerFilter::setGreenGamma(float value) {
  if (_greenGamma == value) {
    return;
  }
  _greenGamma = value;
  invalidateFilter();
}

void LevelsIndividualLayerFilter::setGreenOutputBlack(float value) {
  if (_greenOutputBlack == value) {
    return;
  }
  _greenOutputBlack = value;
  invalidateFilter();
}

void LevelsIndividualLayerFilter::setGreenOutputWhite(float value) {
  if (_greenOutputWhite == value) {
    return;
  }
  _greenOutputWhite = value;
  invalidateFilter();
}

void LevelsIndividualLayerFilter::setBlueInputBlack(float value) {
  if (_blueInputBlack == value) {
    return;
  }
  _blueInputBlack = value;
  invalidateFilter();
}

void LevelsIndividualLayerFilter::setBlueInputWhite(float value) {
  if (_blueInputWhite == value) {
    return;
  }
  _blueInputWhite = value;
  invalidateFilter();
}

void LevelsIndividualLayerFilter::setBlueGamma(float value) {
  if (_blueGamma == value) {
    return;
  }
  _blueGamma = value;
  invalidateFilter();
}

void LevelsIndividualLayerFilter::setBlueOutputBlack(float value) {
  if (_blueOutputBlack == value) {
    return;
  }
  _blueOutputBlack = value;
  invalidateFilter();
}

void LevelsIndividualLayerFilter::setBlueOutputWhite(float value) {
  if (_blueOutputWhite == value) {
    return;
  }
  _blueOutputWhite = value;
  invalidateFilter();
}

std::shared_ptr<tgfx::Image> LevelsIndividualLayerFilter::onFilterImage(
    std::shared_ptr<tgfx::Image> input, float, const tgfx::Rect&, tgfx::Point* offset) {
  if (input == nullptr) {
    return nullptr;
  }
  // Mirrors the legacy LevelsIndividualFilter::Apply: the twenty level values build the
  // per-channel param struct and the RuntimeFilter shader applies the remap.
  pag::LevelsIndividualFilterParam param = {};
  // Channel order in the param struct: 0 red, 1 green, 2 blue, 3 global.
  param.inBlack[3] = _inputBlack;
  param.inWhite[3] = _inputWhite;
  param.gamma[3] = _gamma;
  param.outBlack[3] = _outputBlack;
  param.outWhite[3] = _outputWhite;
  param.inBlack[0] = _redInputBlack;
  param.inWhite[0] = _redInputWhite;
  param.gamma[0] = _redGamma;
  param.outBlack[0] = _redOutputBlack;
  param.outWhite[0] = _redOutputWhite;
  param.inBlack[1] = _greenInputBlack;
  param.inWhite[1] = _greenInputWhite;
  param.gamma[1] = _greenGamma;
  param.outBlack[1] = _greenOutputBlack;
  param.outWhite[1] = _greenOutputWhite;
  param.inBlack[2] = _blueInputBlack;
  param.inWhite[2] = _blueInputWhite;
  param.gamma[2] = _blueGamma;
  param.outBlack[2] = _blueOutputBlack;
  param.outWhite[2] = _blueOutputWhite;
  auto filter = std::make_shared<pag::LevelsIndividualFilter>(nullptr, param);
  return input->makeWithFilter(tgfx::ImageFilter::Runtime(std::move(filter)), offset);
}

}  // namespace pagx
