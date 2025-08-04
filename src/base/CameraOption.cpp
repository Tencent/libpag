/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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
CameraOption::~CameraOption() {
  delete zoom;
  delete depthOfField;
  delete focusDistance;
  delete aperture;
  delete blurLevel;
  delete irisShape;
  delete irisRotation;
  delete irisRoundness;
  delete irisAspectRatio;
  delete irisDiffractionFringe;
  delete highlightGain;
  delete highlightThreshold;
  delete highlightSaturation;
}

void CameraOption::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const {
  zoom->excludeVaryingRanges(timeRanges);
  depthOfField->excludeVaryingRanges(timeRanges);
  focusDistance->excludeVaryingRanges(timeRanges);
  aperture->excludeVaryingRanges(timeRanges);
  blurLevel->excludeVaryingRanges(timeRanges);
  irisShape->excludeVaryingRanges(timeRanges);
  irisRotation->excludeVaryingRanges(timeRanges);
  irisRoundness->excludeVaryingRanges(timeRanges);
  irisAspectRatio->excludeVaryingRanges(timeRanges);
  irisDiffractionFringe->excludeVaryingRanges(timeRanges);
  highlightGain->excludeVaryingRanges(timeRanges);
  highlightThreshold->excludeVaryingRanges(timeRanges);
  highlightSaturation->excludeVaryingRanges(timeRanges);
}

bool CameraOption::verify() const {
  VerifyAndReturn(zoom != nullptr && depthOfField != nullptr && focusDistance != nullptr &&
                  aperture != nullptr && blurLevel != nullptr && irisShape != nullptr &&
                  irisRotation != nullptr && irisRoundness != nullptr &&
                  irisAspectRatio != nullptr && irisDiffractionFringe != nullptr &&
                  highlightGain != nullptr && highlightThreshold != nullptr &&
                  highlightSaturation != nullptr);
}
}  // namespace pag
