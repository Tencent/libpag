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

#include <unordered_map>
#include "base/utils/UniqueID.h"
#include "base/utils/Verify.h"
#include "pag/file.h"

namespace pag {
Composition::Composition() : uniqueID(UniqueID::Next()) {
}

Composition::~Composition() {
  delete cache;
  delete audioBytes;
  for (auto& marker : audioMarkers) {
    delete marker;
  }
}

bool Composition::staticContent() const {
  return staticTimeRanges.size() == 1 && staticTimeRanges.front().start == 0 &&
         staticTimeRanges.front().end == duration - 1;
}

bool Composition::hasImageContent() const {
  return false;
}

CompositionType Composition::type() const {
  return CompositionType::Unknown;
}

void Composition::updateStaticTimeRanges() {
  TimeRange timeRange = {0, duration - 1};
  staticTimeRanges = {timeRange};
}

bool Composition::verify() const {
  if (audioBytes != nullptr && audioBytes->length() == 0) {
    VerifyFailed();
    return false;
  }
  VerifyAndReturn(width > 0 && height > 0 && duration > 0 && frameRate > 0);
}
}  // namespace pag
