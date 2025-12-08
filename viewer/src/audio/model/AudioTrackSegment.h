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

#pragma once

#include "audio/model/AudioSource.h"
#include "pag/types.h"

namespace pag {

struct AudioTrackSegment {
  explicit AudioTrackSegment(const TimeRange& targetRange);
  AudioTrackSegment(std::shared_ptr<AudioSource> source, int trackID, const TimeRange& sourceRange,
                    const TimeRange& targetRange);
  bool isEmpty() const;

  int sourceTrackID = -1;
  std::shared_ptr<AudioSource> source = nullptr;
  TimeRange sourceRange = {-1, -1};
  TimeRange targetRange = {-1, -1};
};

}  // namespace pag
