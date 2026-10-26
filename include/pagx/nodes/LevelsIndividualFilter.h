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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "pagx/nodes/LayerFilter.h"

namespace pagx {

/**
 * A levels filter (spec 7.5) with per-channel black/white points and gamma, migrated from the
 * legacy LevelsIndividual effect. Each channel carries inputBlack, inputWhite, gamma, outputBlack
 * and outputWhite with the AE defaults 0, 1, 1, 0 and 1.
 */
class LevelsIndividualFilter : public LayerFilter {
 public:
  float inputBlack = 0.0f;
  float inputWhite = 1.0f;
  float gamma = 1.0f;
  float outputBlack = 0.0f;
  float outputWhite = 1.0f;

  float redInputBlack = 0.0f;
  float redInputWhite = 1.0f;
  float redGamma = 1.0f;
  float redOutputBlack = 0.0f;
  float redOutputWhite = 1.0f;

  float greenInputBlack = 0.0f;
  float greenInputWhite = 1.0f;
  float greenGamma = 1.0f;
  float greenOutputBlack = 0.0f;
  float greenOutputWhite = 1.0f;

  float blueInputBlack = 0.0f;
  float blueInputWhite = 1.0f;
  float blueGamma = 1.0f;
  float blueOutputBlack = 0.0f;
  float blueOutputWhite = 1.0f;

  NodeType nodeType() const override {
    return NodeType::LevelsIndividualFilter;
  }

 private:
  LevelsIndividualFilter() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
