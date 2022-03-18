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

#pragma once
#ifdef FILE_MOVIE

#include "AudioTrack.h"

namespace pag {
class AudioCompositionTrack : public AudioTrack {
 public:
  explicit AudioCompositionTrack(int trackID) : AudioTrack(trackID) {
  }

  std::string type() const override {
    return "AudioCompositionTrack";
  }

  /**
   * Inserts a time range of a source track into a track of a composition.
   */
  void insertTimeRange(const TimeRange& timeRange, const AudioTrack& ofTrack, int64_t atTime);

  /**
   * Removes a specified time range from the receiver.
   */
  void removeTimeRange(const TimeRange& timeRange);

  /**
   * Changes the duration of a time range in the receiver.
   */
  void scaleTimeRange(const TimeRange& timeRange, int64_t toDuration);

  /**
   * Sets a volume ramp to apply during a specified time range.
   */
  void setVolumeRamp(float fromStartVolume, float toEndVolume, const TimeRange& forTimeRange);

 private:
  std::list<std::tuple<TimeRange, std::tuple<float, float>>> volumeRampList;

  void insertSegments(const std::list<AudioTrackSegment>& segments, int64_t atTime);

  void removeTimeRangeInternal(const TimeRange& timeRange);

  void scaleTimeRangeInternal(const TimeRange& timeRange, int64_t toDuration);

  AudioTrackSegment* segmentTargetContainTimeRange(std::list<AudioTrackSegment>::iterator& it,
                                                   const TimeRange& timeRange,
                                                   bool hasIntersecting);

  AudioTrackSegment* segmentTargetIntersectTimeRange(std::list<AudioTrackSegment>::iterator& it,
                                                     const TimeRange& timeRange,
                                                     bool hasIntersecting);

  friend class AudioSmoothVolume;
};
}  // namespace pag

#endif
