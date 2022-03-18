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

#ifdef FILE_MOVIE

#include "AudioTrack.h"
#include "AudioTrackSegment.h"

namespace pag {
int64_t AudioTrack::duration() const {
  return _segments.empty() ? 0 : _segments.back().timeMapping.target.end;
}

std::list<AudioTrackSegment> AudioTrack::segmentsForTimeRange(TimeRange timeRange) const {
  std::list<AudioTrackSegment> result;
  if (_segments.empty() || !timeRange.isValid()) {
    return result;
  }
  if (_segments.back().timeMapping.target.end < timeRange.start) {
    return result;
  }
  for (auto& seg : _segments) {
    auto timeMapping = seg.timeMapping;
    if (timeRange.end <= timeMapping.target.start || timeMapping.target.end <= timeRange.start) {
      continue;
    }
    auto segSource = timeMapping.source;
    auto segTarget = timeMapping.target;
    auto targetStart = std::max(timeRange.start, segTarget.start);
    auto targetEnd = std::min(timeRange.end, segTarget.end);
    auto sourceStart = static_cast<int64_t>(static_cast<double>(targetStart - segTarget.start) *
                                                static_cast<double>(segSource.duration()) /
                                                static_cast<double>(segTarget.duration()) +
                                            segSource.start);
    auto sourceEnd = static_cast<int64_t>(static_cast<double>(targetEnd - segTarget.start) *
                                              static_cast<double>(segSource.duration()) /
                                              static_cast<double>(segTarget.duration()) +
                                          segSource.start);
    result.emplace_back(seg.source, seg.sourceTrackID, TimeRange{sourceStart, sourceEnd},
                        TimeRange{targetStart, targetEnd});
  }
  return result;
}
}  // namespace pag
#endif
