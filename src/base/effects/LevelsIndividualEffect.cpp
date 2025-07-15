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

#include "base/utils/Verify.h"
#include "pag/file.h"

namespace pag {
LevelsIndividualEffect::~LevelsIndividualEffect() {
  // RGB
  delete inputBlack;
  delete inputWhite;
  delete gamma;
  delete outputBlack;
  delete outputWhite;
  // Red
  delete redInputBlack;
  delete redInputWhite;
  delete redGamma;
  delete redOutputBlack;
  delete redOutputWhite;
  // Green
  delete greenInputBlack;
  delete greenInputWhite;
  delete greenGamma;
  delete greenOutputBlack;
  delete greenOutputWhite;
  // Blue
  delete blueInputBlack;
  delete blueInputWhite;
  delete blueGamma;
  delete blueOutputBlack;
  delete blueOutputWhite;
}

bool LevelsIndividualEffect::visibleAt(Frame) const {
  return true;
}

void LevelsIndividualEffect::transformBounds(Rect*, const Point&, Frame) const {
}

void LevelsIndividualEffect::excludeVaryingRanges(std::vector<pag::TimeRange>* timeRanges) const {
  Effect::excludeVaryingRanges(timeRanges);
  // RGB
  inputBlack->excludeVaryingRanges(timeRanges);
  inputWhite->excludeVaryingRanges(timeRanges);
  gamma->excludeVaryingRanges(timeRanges);
  outputBlack->excludeVaryingRanges(timeRanges);
  outputWhite->excludeVaryingRanges(timeRanges);
  // Red
  redInputBlack->excludeVaryingRanges(timeRanges);
  redInputWhite->excludeVaryingRanges(timeRanges);
  redGamma->excludeVaryingRanges(timeRanges);
  redOutputBlack->excludeVaryingRanges(timeRanges);
  redOutputWhite->excludeVaryingRanges(timeRanges);
  // Green
  greenInputBlack->excludeVaryingRanges(timeRanges);
  greenInputWhite->excludeVaryingRanges(timeRanges);
  greenGamma->excludeVaryingRanges(timeRanges);
  greenOutputBlack->excludeVaryingRanges(timeRanges);
  greenOutputWhite->excludeVaryingRanges(timeRanges);
  // Blue
  blueInputBlack->excludeVaryingRanges(timeRanges);
  blueInputWhite->excludeVaryingRanges(timeRanges);
  blueGamma->excludeVaryingRanges(timeRanges);
  blueOutputBlack->excludeVaryingRanges(timeRanges);
  blueOutputWhite->excludeVaryingRanges(timeRanges);
}

static bool calculate5and(bool a, bool b, bool c, bool d, bool e) {
  return a && b && c && d && e;
}

bool LevelsIndividualEffect::verify() const {
  if (!Effect::verify()) {
    VerifyFailed();
    return false;
  }
  auto rgb = calculate5and(inputBlack, inputWhite, gamma, outputBlack, outputWhite);
  auto red = calculate5and(redInputBlack, redInputWhite, redGamma, redOutputBlack, redOutputWhite);
  auto green = calculate5and(greenInputBlack, greenInputWhite, greenGamma, greenOutputBlack,
                             greenOutputWhite);
  auto blue =
      calculate5and(blueInputBlack, blueInputWhite, blueGamma, blueOutputBlack, blueOutputWhite);
  VerifyAndReturn(calculate5and(rgb, red, green, blue, 1));
}
}  // namespace pag
