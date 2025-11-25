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

#include "AudioTrack.h"
#include "AudioTrackSegment.h"

namespace pag {

static void SortSegments(std::list<AudioTrackSegment>& segments) {
  int64_t start = 0;
  for (auto& segment : segments) {
    auto target = segment.targetRange;
    segment.targetRange = {start, target.end - target.start + start};
    start = segment.targetRange.end;
  }
}

static void ScaleSegment(AudioTrackSegment* segment, double ratio) {
  if (segment == nullptr) {
    return;
  }
  auto target = segment->targetRange;
  auto duration = static_cast<double>(target.duration()) / ratio;
  segment->targetRange = {target.start, static_cast<int64_t>(target.start + duration)};
}

AudioTrack::AudioTrack(int trackID) : trackID(trackID) {
}

std::string AudioTrack::type() const {
  return "AudioTrack";
}

int AudioTrack::getTrackID() const {
  return trackID;
}

int64_t AudioTrack::duration() const {
  return segments.empty() ? 0 : segments.back().targetRange.end;
}

std::list<AudioTrackSegment> AudioTrack::getSegmentsByTimeRange(TimeRange timeRange) const {
  std::list<AudioTrackSegment> result;
  if (segments.empty() || !timeRange.isValid()) {
    return result;
  }
  if (segments.back().targetRange.end < timeRange.start) {
    return result;
  }
  for (auto& segment : segments) {
    if (timeRange.end <= segment.targetRange.start || segment.targetRange.end <= timeRange.start) {
      continue;
    }
    auto newTargetStart = std::max(timeRange.start, segment.targetRange.start);
    auto newTargetEnd = std::min(timeRange.end, segment.targetRange.end);
    auto newSourceStart =
        static_cast<int64_t>(static_cast<double>((newTargetStart - segment.targetRange.start) *
                                                 segment.sourceRange.duration()) /
                                 static_cast<double>(segment.targetRange.duration()) +
                             segment.sourceRange.start);
    auto newSourceEnd =
        static_cast<int64_t>(static_cast<double>((newTargetEnd - segment.targetRange.start) *
                                                 segment.sourceRange.duration()) /
                                 static_cast<double>(segment.targetRange.duration()) +
                             segment.sourceRange.start);

    result.emplace_back(segment.source, segment.sourceTrackID,
                        TimeRange{newSourceStart, newSourceEnd},
                        TimeRange{newTargetStart, newTargetEnd});
  }
  return result;
}

void AudioTrack::addSegment(const AudioTrackSegment& segment) {
  segments.push_back(segment);
}

void AudioTrack::insetTimeRange(const TimeRange& timeRange, const AudioTrack& track, int64_t time) {
  if (!timeRange.isValid()) {
    return;
  }
  auto segments = track.getSegmentsByTimeRange(timeRange);
  if (segments.empty()) {
    return;
  }
  insertSegments(segments, time);
}

void AudioTrack::removeTimeRange(const TimeRange& timeRange) {
  if (!timeRange.isValid() || segments.empty()) {
    return;
  }
  if (segments.back().targetRange.end < timeRange.start) {
    return;
  }
  removeTimeRangeInternal(timeRange);
}

void AudioTrack::scaleTimeRange(const TimeRange& timeRange, int64_t duration) {
  if (!timeRange.isValid() || segments.empty()) {
    return;
  }
  if (duration <= 0 || timeRange.duration() == duration) {
    return;
  }
  if (segments.back().targetRange.end < timeRange.start) {
    return;
  }
  scaleTimeRangeInternal(timeRange, duration);
}

void AudioTrack::insertSegments(const std::list<AudioTrackSegment>& segments, int64_t time) {
  if (this->segments.empty()) {
    if (time > 0) {
      this->segments.emplace_back(TimeRange{0, time});
    }
    this->segments.insert(this->segments.end(), segments.begin(), segments.end());
    SortSegments(this->segments);
    return;
  }
  if (time == 0) {
    this->segments.insert(this->segments.begin(), segments.begin(), segments.end());
    SortSegments(this->segments);
    return;
  }
  auto lastEnd = this->segments.back().targetRange.end;
  if (lastEnd == time) {
    this->segments.insert(this->segments.end(), segments.begin(), segments.end());
    SortSegments(this->segments);
    return;
  }
  if (lastEnd < time) {
    this->segments.emplace_back(TimeRange{lastEnd, time});
    this->segments.insert(this->segments.end(), segments.begin(), segments.end());
    SortSegments(this->segments);
    return;
  }
  AudioTrackSegment* tempSegment = nullptr;
  auto index = this->segments.begin();
  for (auto& segment : this->segments) {
    if (segment.targetRange.contains(time)) {
      tempSegment = &segment;
      break;
    }
    index++;
  }
  if (tempSegment == nullptr) {
    return;
  }
  if (tempSegment->targetRange.start < time) {
    auto segmentSource = tempSegment->sourceRange;
    auto segmentTarget = tempSegment->targetRange;
    auto segment1Target = TimeRange{segmentTarget.start, time};
    auto segment2Target = TimeRange{segment1Target.end, segmentTarget.end};
    auto seg1SourceEnd =
        (time - segmentTarget.start) * segmentSource.duration() / segmentTarget.duration() +
        segmentSource.start;
    auto segment1Source = TimeRange{segmentSource.start, seg1SourceEnd};
    auto segment2Source = TimeRange{segment1Source.end, segmentSource.end};
    tempSegment->sourceRange = segment1Source;
    tempSegment->targetRange = segment1Target;
    index = this->segments.emplace(++index, tempSegment->source, tempSegment->sourceTrackID,
                                   segment2Source, segment2Target);
  }
  this->segments.insert(index, segments.begin(), segments.end());
  SortSegments(this->segments);
}

void AudioTrack::removeTimeRangeInternal(const TimeRange& timeRange) {
  for (auto it = segments.begin(); it != segments.end(); ++it) {
    auto target = (*it).targetRange;
    if (timeRange.end <= target.start || target.end <= timeRange.start) {
      continue;
    }
    if (timeRange.start <= target.start && target.end <= timeRange.end) {
      segments.erase(it--);
      continue;
    }
    if (target.start <= timeRange.start && timeRange.end <= target.end) {
      segmentTargetContainTimeRange(it, timeRange, false);
      continue;
    }
    segmentTargetIntersectTimeRange(it, timeRange, false);
  }
  SortSegments(segments);
}

void AudioTrack::scaleTimeRangeInternal(const TimeRange& timeRange, int64_t duration) {
  auto ratio = static_cast<double>(timeRange.duration()) / static_cast<double>(duration);
  for (auto it = segments.begin(); it != segments.end(); ++it) {
    auto target = (*it).targetRange;
    if (timeRange.end <= target.start || target.end <= timeRange.start) {
      continue;
    }
    if (timeRange.start <= target.start && target.end <= timeRange.end) {
      ScaleSegment(&*it, ratio);
      continue;
    }
    if (target.start <= timeRange.start && timeRange.end <= target.end) {
      auto seg2 = segmentTargetContainTimeRange(it, timeRange, true);
      ScaleSegment(seg2, ratio);
      continue;
    }
    auto seg2 = segmentTargetIntersectTimeRange(it, timeRange, true);
    ScaleSegment(seg2, ratio);
  }
  SortSegments(segments);
}

AudioTrackSegment* AudioTrack::segmentTargetContainTimeRange(
    std::list<AudioTrackSegment>::iterator& it, const TimeRange& timeRange, bool hasIntersecting) {
  auto& segment = *it;
  auto sourceTimeRange = segment.sourceRange;
  auto targetTimeRange = segment.targetRange;
  auto segment1TargetStart = targetTimeRange.start;
  auto segment1TargetEnd = timeRange.start;
  segment.sourceRange.start = (segment1TargetStart - targetTimeRange.start) *
                                  sourceTimeRange.duration() / targetTimeRange.duration() +
                              sourceTimeRange.start;
  segment.sourceRange.end = (segment1TargetEnd - targetTimeRange.start) *
                                sourceTimeRange.duration() / targetTimeRange.duration() +
                            sourceTimeRange.start;
  segment.targetRange = {segment1TargetStart, segment1TargetEnd};

  AudioTrackSegment* segment2 = nullptr;
  if (hasIntersecting) {
    auto segment2TargetStart = timeRange.start;
    auto segment2TargetEnd = timeRange.end;
    auto segment2SourceStart = (segment2TargetStart - targetTimeRange.start) *
                                   sourceTimeRange.duration() / targetTimeRange.duration() +
                               sourceTimeRange.start;
    auto segment2SourceEnd = (segment2TargetEnd - targetTimeRange.start) *
                                 sourceTimeRange.duration() / targetTimeRange.duration() +
                             sourceTimeRange.start;
    it = segments.emplace(++it, segment.source, segment.sourceTrackID,
                          TimeRange{segment2SourceStart, segment2SourceEnd},
                          TimeRange{segment2TargetStart, segment2TargetEnd});
    segment2 = &*it;
  }

  auto segment3TargetStart = timeRange.end;
  auto segment3TargetEnd = targetTimeRange.end;
  auto segment3SourceStart = (segment3TargetStart - targetTimeRange.start) *
                                 sourceTimeRange.duration() / targetTimeRange.duration() +
                             sourceTimeRange.start;
  auto segment3SourceEnd = (segment3TargetEnd - targetTimeRange.start) *
                               sourceTimeRange.duration() / targetTimeRange.duration() +
                           sourceTimeRange.start;
  it = segments.emplace(++it, segment.source, segment.sourceTrackID,
                        TimeRange{segment3SourceStart, segment3SourceEnd},
                        TimeRange{segment3TargetStart, segment3TargetEnd});
  return segment2;
}

AudioTrackSegment* AudioTrack::segmentTargetIntersectTimeRange(
    std::list<AudioTrackSegment>::iterator& it, const TimeRange& timeRange, bool hasIntersecting) {
  auto& segment = *it;
  auto segmentSource = segment.sourceRange;
  auto segmentTarget = segment.targetRange;
  int64_t segment1TargetStart = 0;
  int64_t segment1TargetEnd = 0;
  if (segmentTarget.start < timeRange.start && timeRange.start < segmentTarget.end) {
    segment1TargetStart = segmentTarget.start;
    segment1TargetEnd = timeRange.start;
  } else {
    segment1TargetStart = timeRange.end;
    segment1TargetEnd = segmentTarget.end;
  }
  auto segment1SourceStart = (segment1TargetStart - segmentTarget.start) *
                                 segmentSource.duration() / segmentTarget.duration() +
                             segmentSource.start;
  auto segment1SourceEnd = (segment1TargetEnd - segmentTarget.start) * segmentSource.duration() /
                               segmentTarget.duration() +
                           segmentSource.start;
  segment.targetRange = {segment1TargetStart, segment1TargetEnd};
  segment.sourceRange = {segment1SourceStart, segment1SourceEnd};

  AudioTrackSegment* segment2 = nullptr;
  if (!hasIntersecting) {
    return segment2;
  }
  int64_t segment2TargetStart = 0;
  int64_t segment2TargetEnd = 0;
  if (segmentTarget.start < timeRange.start && timeRange.start < segmentTarget.end) {
    segment2TargetStart = timeRange.start;
    segment2TargetEnd = segmentTarget.end;
  } else {
    segment2TargetStart = segmentTarget.start;
    segment2TargetEnd = timeRange.end;
  }
  auto segment2SourceStart = (segment2TargetStart - segmentTarget.start) *
                                 segmentSource.duration() / segmentTarget.duration() +
                             segmentSource.start;
  auto segment2SourceEnd = (segment2TargetEnd - segmentTarget.start) * segmentSource.duration() /
                               segmentTarget.duration() +
                           segmentSource.start;
  auto tempSegment = AudioTrackSegment(segment.source, segment.sourceTrackID,
                                       TimeRange{segment2SourceStart, segment2SourceEnd},
                                       TimeRange{segment2TargetStart, segment2TargetEnd});
  if (segment1TargetStart < segment2TargetStart) {
    it = segments.insert(++it, tempSegment);
    segment2 = &*it;
  } else {
    it = segments.insert(it, tempSegment);
    segment2 = &*it;
    it++;
  }
  return segment2;
}

}  // namespace pag
