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

#include <list>
#include "AudioTrackSegment.h"
#include "pag/types.h"

namespace pag {

class AudioTrack {
 public:
  explicit AudioTrack(int trackID);
  std::string type() const;

  int getTrackID() const;
  int64_t duration() const;
  std::list<AudioTrackSegment> getSegmentsByTimeRange(TimeRange timeRange) const;
  void addSegment(const AudioTrackSegment& segment);
  void insetTimeRange(const TimeRange& timeRange, const AudioTrack& track, int64_t time);
  void removeTimeRange(const TimeRange& timeRange);
  void scaleTimeRange(const TimeRange& timeRange, int64_t duration);

 protected:
  void insertSegments(const std::list<AudioTrackSegment>& segments, int64_t time);
  void removeTimeRangeInternal(const TimeRange& timeRange);
  void scaleTimeRangeInternal(const TimeRange& timeRange, int64_t duration);
  AudioTrackSegment* segmentTargetContainTimeRange(std::list<AudioTrackSegment>::iterator& it,
                                                   const TimeRange& timeRange,
                                                   bool hasIntersecting);
  AudioTrackSegment* segmentTargetIntersectTimeRange(std::list<AudioTrackSegment>::iterator& it,
                                                     const TimeRange& timeRange,
                                                     bool hasIntersecting);

  int trackID = -1;
  std::list<AudioTrackSegment> segments = {};

  friend class AudioTrackReader;
};

}  // namespace pag
