/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#include "AudioTrackSegment.h"
#include <utility>

namespace pag {

AudioTrackSegment::AudioTrackSegment(std::shared_ptr<AudioSource> source, int trackID,
                                     const TimeRange& sourceRange, const TimeRange& targetRanget)
    : sourceTrackID(trackID), source(std::move(source)), sourceRange(sourceRange),
      targetRange(targetRanget) {
}

AudioTrackSegment::AudioTrackSegment(const TimeRange& targetRange) : targetRange(targetRange) {
}

bool AudioTrackSegment::isEmpty() const {
  return sourceRange.start == sourceRange.end && targetRange.start == targetRange.end;
}

}  // namespace pag
