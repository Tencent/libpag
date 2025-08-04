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
TextWigglySelector::~TextWigglySelector() {
  delete mode;
  delete maxAmount;
  delete minAmount;
  delete wigglesPerSecond;
  delete correlation;
  delete temporalPhase;
  delete spatialPhase;
  delete lockDimensions;
  delete randomSeed;
}

void TextWigglySelector::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const {
  // there are no static time ranges for TextWigglySelector
  timeRanges->erase(timeRanges->begin(), timeRanges->end());
}

bool TextWigglySelector::verify() const {
  VerifyAndReturn(mode != nullptr && maxAmount != nullptr && minAmount != nullptr &&
                  wigglesPerSecond != nullptr && correlation != nullptr &&
                  temporalPhase != nullptr && spatialPhase != nullptr &&
                  lockDimensions != nullptr && randomSeed != nullptr);
}
}  // namespace pag
