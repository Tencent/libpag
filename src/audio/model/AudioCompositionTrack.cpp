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

#include "AudioCompositionTrack.h"

namespace pag {
void AudioCompositionTrack::insertTimeRange(const TimeRange& timeRange, const AudioTrack& ofTrack,
                                            int64_t atTime) {
  if (!timeRange.isValid()) {
    return;
  }
  auto segs = ofTrack.segmentsForTimeRange(timeRange);
  if (segs.empty()) {
    return;
  }
  insertSegments(segs, atTime);
}

void ScaleSegment(AudioTrackSegment* segment, double speed) {
  if (segment == nullptr) {
    return;
  }
  auto target = segment->timeMapping.target;
  auto duration = static_cast<double>(target.duration()) / speed;
  segment->timeMapping.target = {target.start, static_cast<int64_t>(target.start + duration)};
}

void SortSegments(std::list<AudioTrackSegment>& segments) {
  int64_t start = 0;
  for (auto& seg : segments) {
    auto target = seg.timeMapping.target;
    seg.timeMapping.target = {start, target.duration() + start};
    start = seg.timeMapping.target.end;
  }
}

void AudioCompositionTrack::insertSegments(const std::list<AudioTrackSegment>& segments,
                                           int64_t atTime) {
  // 1 数组为空的情况
  if (_segments.empty()) {
    if (atTime > 0) {
      // 添加一段空 segment
      _segments.emplace_back(TimeRange{0, atTime});
    }
    _segments.insert(_segments.end(), segments.begin(), segments.end());
    SortSegments(_segments);
    return;
  }
  // 2 数组不为空，插在头部的情况
  if (atTime == 0) {
    _segments.insert(_segments.begin(), segments.begin(), segments.end());
    SortSegments(_segments);
    return;
  }
  // 3 数组不为空，插在尾部的情况
  auto lastEnd = _segments.back().timeMapping.target.end;
  // 3-1 刚好接在尾部的情况
  if (lastEnd == atTime) {
    _segments.insert(_segments.end(), segments.begin(), segments.end());
    SortSegments(_segments);
    return;
  }
  // 3-2 插在尾部，没有衔接的情况
  if (lastEnd < atTime) {
    _segments.emplace_back(TimeRange{lastEnd, atTime});
    _segments.insert(_segments.end(), segments.begin(), segments.end());
    SortSegments(_segments);
    return;
  }
  // 4 数组不为空，插在中间的情况
  AudioTrackSegment* tempSeg = nullptr;
  auto index = _segments.begin();
  for (auto& seg : _segments) {
    if (seg.timeMapping.target.contains(atTime)) {
      tempSeg = &seg;
      break;
    }
    index++;
  }
  if (tempSeg == nullptr) {
    return;
  }
  // 4-1 需要分割 segment
  if (tempSeg->timeMapping.target.start < atTime) {
    auto segSource = tempSeg->timeMapping.source;
    auto segTarget = tempSeg->timeMapping.target;
    auto seg1Target = TimeRange{segTarget.start, atTime};
    auto seg2Target = TimeRange{seg1Target.end, segTarget.end};
    auto seg1SourceEnd =
        (atTime - segTarget.start) * segSource.duration() / segTarget.duration() + segSource.start;
    auto seg1Source = TimeRange{segSource.start, seg1SourceEnd};
    auto seg2Source = TimeRange{seg1Source.end, segSource.end};
    tempSeg->timeMapping.source = seg1Source;
    tempSeg->timeMapping.target = seg1Target;
    index =
        _segments.emplace(++index, tempSeg->source, tempSeg->sourceTrackID, seg2Source, seg2Target);
  }
  _segments.insert(index, segments.begin(), segments.end());
  SortSegments(_segments);
}

void AudioCompositionTrack::removeTimeRange(const TimeRange& timeRange) {
  if (!timeRange.isValid() || _segments.empty()) {
    return;
  }
  if (_segments.back().timeMapping.target.end < timeRange.start) {
    return;
  }
  removeTimeRangeInternal(timeRange);
}

void AudioCompositionTrack::removeTimeRangeInternal(const TimeRange& timeRange) {
  for (auto it = _segments.begin(); it != _segments.end(); ++it) {
    auto target = (*it).timeMapping.target;
    if (timeRange.end <= target.start || target.end <= timeRange.start) {
      // target outside timeRange
      continue;
    }
    if (timeRange.start <= target.start && target.end <= timeRange.end) {
      // target inside timeRange
      _segments.erase(it--);
      continue;
    }
    if (target.start <= timeRange.start && timeRange.end <= target.end) {
      // target contain timeRange
      segmentTargetContainTimeRange(it, timeRange, false);
      continue;
    }
    // target intersect timeRange
    segmentTargetIntersectTimeRange(it, timeRange, false);
  }
  SortSegments(_segments);
}

void AudioCompositionTrack::scaleTimeRange(const TimeRange& timeRange, int64_t toDuration) {
  if (!timeRange.isValid() || _segments.empty()) {
    return;
  }
  if (timeRange.duration() == toDuration) {
    return;
  }
  if (_segments.back().timeMapping.target.end < timeRange.start) {
    return;
  }
  if (toDuration <= 0) {
    return;
  }
  scaleTimeRangeInternal(timeRange, toDuration);
}

void AudioCompositionTrack::scaleTimeRangeInternal(const TimeRange& timeRange, int64_t toDuration) {
  auto speed = static_cast<double>(timeRange.duration()) / static_cast<double>(toDuration);
  for (auto it = _segments.begin(); it != _segments.end(); ++it) {
    auto target = (*it).timeMapping.target;
    if (timeRange.end <= target.start || target.end <= timeRange.start) {
      // target outside timeRange
      continue;
    }
    if (timeRange.start <= target.start && target.end <= timeRange.end) {
      // target inside timeRange
      ScaleSegment(&*it, speed);
      continue;
    }
    if (target.start <= timeRange.start && timeRange.end <= target.end) {
      // target contain timeRange
      auto seg2 = segmentTargetContainTimeRange(it, timeRange, true);
      ScaleSegment(seg2, speed);
      continue;
    }
    // target intersect timeRange
    auto seg2 = segmentTargetIntersectTimeRange(it, timeRange, true);
    ScaleSegment(seg2, speed);
  }
  SortSegments(_segments);
}

template <typename T>
void SetValueInternal(const T& value, TimeRange forTimeRange,
                      std::list<std::tuple<TimeRange, T>>& inList) {
  for (auto it = inList.begin(); it != inList.end(); ++it) {
    auto& range = std::get<0>(*it);
    if (forTimeRange.end <= range.start || range.end <= forTimeRange.start) {
      continue;
    } else if (forTimeRange.start <= range.start && range.end <= forTimeRange.end) {
      inList.erase(it--);
      continue;
    } else if (range.start < forTimeRange.start && forTimeRange.end < range.end) {
      auto tuple = std::tuple<TimeRange, T>({forTimeRange.end, range.end}, std::get<1>(*it));
      range.end = forTimeRange.start;
      it = inList.insert(++it, tuple);
    } else if (range.start < forTimeRange.start && forTimeRange.start < range.end) {
      range.end = forTimeRange.start;
    } else {
      range.start = forTimeRange.end;
    }
  }
  inList.emplace_back(forTimeRange, value);
}

template <typename T>
void SetValue(const T& value, TimeRange forTimeRange, std::list<std::tuple<TimeRange, T>>& inList) {
  if (!forTimeRange.isValid()) {
    return;
  }
  if (inList.empty()) {
    inList.emplace_back(forTimeRange, value);
    return;
  }
  SetValueInternal(value, forTimeRange, inList);
}

template <typename T>
void Sort(std::list<std::tuple<TimeRange, T>>& inList) {
  inList.sort([](std::tuple<TimeRange, T> volumeRamp1, std::tuple<TimeRange, T> volumeRamp2) {
    return std::get<0>(volumeRamp1).start < std::get<0>(volumeRamp2).start;
  });
}

void AudioCompositionTrack::setVolumeRamp(float fromStartVolume, float toEndVolume,
                                          const TimeRange& forTimeRange) {
  SetValue(std::tuple<float, float>(fromStartVolume, toEndVolume), forTimeRange, volumeRampList);
  Sort(volumeRampList);
}

/**
 * 当 seg.target 包含 timeRange 时，分割 seg 然后放入 segments 中
 * @param it 被分割对象的位置
 * @param timeRange 时间区间
 * @param hasIntersecting 是否把相交的部分放到 segments 中，false 为不放，true 为放
 * @return 相交的 seg，如果没有，返回空指针
 */
AudioTrackSegment* AudioCompositionTrack::segmentTargetContainTimeRange(
    std::list<AudioTrackSegment>::iterator& it, const TimeRange& timeRange, bool hasIntersecting) {
  auto& seg = *it;
  auto segSource = seg.timeMapping.source;
  auto segTarget = seg.timeMapping.target;
  auto seg1TargetStart = segTarget.start;
  auto seg1TargetEnd = timeRange.start;
  auto seg1SourceStart =
      (seg1TargetStart - segTarget.start) * segSource.duration() / segTarget.duration() +
      segSource.start;
  auto seg1SourceEnd =
      (seg1TargetEnd - segTarget.start) * segSource.duration() / segTarget.duration() +
      segSource.start;
  seg.timeMapping.target = {seg1TargetStart, seg1TargetEnd};
  seg.timeMapping.source = {seg1SourceStart, seg1SourceEnd};

  AudioTrackSegment* seg2 = nullptr;
  if (hasIntersecting) {
    auto seg2TargetStart = timeRange.start;
    auto seg2TargetEnd = timeRange.end;
    auto seg2SourceStart =
        (seg2TargetStart - segTarget.start) * segSource.duration() / segTarget.duration() +
        segSource.start;
    auto seg2SourceEnd =
        (seg2TargetEnd - segTarget.start) * segSource.duration() / segTarget.duration() +
        segSource.start;
    it = _segments.emplace(++it, seg.source, seg.sourceTrackID,
                           TimeRange{seg2SourceStart, seg2SourceEnd},
                           TimeRange{seg2TargetStart, seg2TargetEnd});
    seg2 = &*it;
  }

  auto seg3TargetStart = timeRange.end;
  auto seg3TargetEnd = segTarget.end;
  auto seg3SourceStart =
      (seg3TargetStart - segTarget.start) * segSource.duration() / segTarget.duration() +
      segSource.start;
  auto seg3SourceEnd =
      (seg3TargetEnd - segTarget.start) * segSource.duration() / segTarget.duration() +
      segSource.start;
  it = _segments.emplace(++it, seg.source, seg.sourceTrackID,
                         TimeRange{seg3SourceStart, seg3SourceEnd},
                         TimeRange{seg3TargetStart, seg3TargetEnd});
  return seg2;
}

/**
 * 当 seg.target 和 timeRange 相交时，分割 seg 然后放入 segments 中
 * @param it 被分割对象的位置
 * @param timeRange 时间区间
 * @param hasIntersecting 是否把相交的部分放到 segments 中，false 为不放，true 为放
 * @return 相交的 seg，如果没有，返回空指针
 */
AudioTrackSegment* AudioCompositionTrack::segmentTargetIntersectTimeRange(
    std::list<AudioTrackSegment>::iterator& it, const TimeRange& timeRange, bool hasIntersecting) {
  auto& seg = *it;
  auto segSource = seg.timeMapping.source;
  auto segTarget = seg.timeMapping.target;
  int64_t seg1TargetStart;
  int64_t seg1TargetEnd;
  if (segTarget.start < timeRange.start && timeRange.start < segTarget.end) {
    seg1TargetStart = segTarget.start;
    seg1TargetEnd = timeRange.start;
  } else {
    seg1TargetStart = timeRange.end;
    seg1TargetEnd = segTarget.end;
  }
  auto seg1SourceStart =
      (seg1TargetStart - segTarget.start) * segSource.duration() / segTarget.duration() +
      segSource.start;
  auto seg1SourceEnd =
      (seg1TargetEnd - segTarget.start) * segSource.duration() / segTarget.duration() +
      segSource.start;
  seg.timeMapping.target = {seg1TargetStart, seg1TargetEnd};
  seg.timeMapping.source = {seg1SourceStart, seg1SourceEnd};

  AudioTrackSegment* seg2 = nullptr;
  if (!hasIntersecting) {
    return seg2;
  }
  int64_t seg2TargetStart;
  int64_t seg2TargetEnd;
  if (segTarget.start < timeRange.start && timeRange.start < segTarget.end) {
    seg2TargetStart = timeRange.start;
    seg2TargetEnd = segTarget.end;
  } else {
    seg2TargetStart = segTarget.start;
    seg2TargetEnd = timeRange.end;
  }
  auto seg2SourceStart =
      (seg2TargetStart - segTarget.start) * segSource.duration() / segTarget.duration() +
      segSource.start;
  auto seg2SourceEnd =
      (seg2TargetEnd - segTarget.start) * segSource.duration() / segTarget.duration() +
      segSource.start;
  auto tempSegment =
      AudioTrackSegment(seg.source, seg.sourceTrackID, TimeRange{seg2SourceStart, seg2SourceEnd},
                        TimeRange{seg2TargetStart, seg2TargetEnd});
  if (seg1TargetStart < seg2TargetStart) {
    it = _segments.insert(++it, tempSegment);
    seg2 = &*it;
  } else {
    it = _segments.insert(it, tempSegment);
    seg2 = &*it;
    it++;
  }
  return seg2;
}
}  // namespace pag
#endif
