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

#include "base/utils/Verify.h"
#include "pag/file.h"

namespace pag {
TextPathOptions::~TextPathOptions() {
  delete reversedPath;
  delete perpendicularToPath;
  delete forceAlignment;
  delete firstMargin;
  delete lastMargin;
}

void TextPathOptions::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const {
  reversedPath->excludeVaryingRanges(timeRanges);
  perpendicularToPath->excludeVaryingRanges(timeRanges);
  forceAlignment->excludeVaryingRanges(timeRanges);
  firstMargin->excludeVaryingRanges(timeRanges);
  lastMargin->excludeVaryingRanges(timeRanges);
}

bool TextPathOptions::verify() const {
  VerifyAndReturn(reversedPath != nullptr && perpendicularToPath != nullptr &&
                  forceAlignment != nullptr && firstMargin != nullptr && lastMargin != nullptr);
}
}  // namespace pag