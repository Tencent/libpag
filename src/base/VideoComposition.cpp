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
VideoComposition::~VideoComposition() {
  for (auto sequence : sequences) {
    delete sequence;
  }
}

CompositionType VideoComposition::type() const {
  return CompositionType::Video;
}

void VideoComposition::updateStaticTimeRanges() {
  staticTimeRanges = {};
  if (duration <= 1) {
    return;
  }
  if (!sequences.empty()) {
    auto sequence = sequences[0];
    for (size_t i = 1; i < sequences.size(); i++) {
      auto item = sequences[i];
      if (item->frameRate > sequence->frameRate) {
        sequence = item;
      }
    }
    float timeScale = frameRate / sequence->frameRate;
    for (auto timeRange : sequence->staticTimeRanges) {
      timeRange.start = static_cast<Frame>(roundf(timeRange.start * timeScale));
      timeRange.end = static_cast<Frame>(roundf(timeRange.end * timeScale));
      staticTimeRanges.push_back(timeRange);
    }
  } else {
    TimeRange range = {0, duration - 1};
    staticTimeRanges.push_back(range);
  }
}

bool VideoComposition::hasImageContent() const {
  return true;
}

bool VideoComposition::verify() const {
  if (!Composition::verify() || sequences.empty()) {
    VerifyFailed();
    return false;
  }
  auto sequenceValid = [](VideoSequence* sequence) {
    return sequence != nullptr && sequence->verify();
  };
  if (!std::all_of(sequences.begin(), sequences.end(), sequenceValid)) {
    VerifyFailed();
    return false;
  }
  return true;
}
}  // namespace pag
