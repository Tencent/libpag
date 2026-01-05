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
PolyStarElement::~PolyStarElement() {
  delete points;
  delete position;
  delete rotation;
  delete innerRadius;
  delete outerRadius;
  delete innerRoundness;
  delete outerRoundness;
}

void PolyStarElement::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const {
  points->excludeVaryingRanges(timeRanges);
  position->excludeVaryingRanges(timeRanges);
  rotation->excludeVaryingRanges(timeRanges);
  innerRadius->excludeVaryingRanges(timeRanges);
  outerRadius->excludeVaryingRanges(timeRanges);
  innerRoundness->excludeVaryingRanges(timeRanges);
  outerRoundness->excludeVaryingRanges(timeRanges);
}

bool PolyStarElement::verify() const {
  if (!ShapeElement::verify()) {
    VerifyFailed();
    return false;
  }
  VerifyAndReturn(points != nullptr && position != nullptr && rotation != nullptr &&
                  innerRadius != nullptr && outerRadius != nullptr && innerRoundness != nullptr &&
                  outerRoundness != nullptr);
}
}  // namespace pag
