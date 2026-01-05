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
TrimPathsElement::~TrimPathsElement() {
  delete start;
  delete end;
  delete offset;
}

void TrimPathsElement::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) const {
  start->excludeVaryingRanges(timeRanges);
  end->excludeVaryingRanges(timeRanges);
  offset->excludeVaryingRanges(timeRanges);
}

bool TrimPathsElement::verify() const {
  if (!ShapeElement::verify()) {
    VerifyFailed();
    return false;
  }
  VerifyAndReturn(start != nullptr && end != nullptr && offset != nullptr);
}
}  // namespace pag
