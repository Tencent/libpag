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

#include "pag/file.h"
#include "pag/types.h"

namespace pag {
static void TargetInsideTimeRange(std::vector<TimeRange>* timeRanges, int i, Frame startTime,
                                  Frame endTime) {
  auto timeRange = (*timeRanges)[i];
  TimeRange range = {endTime + 1, timeRange.end};
  timeRange.end = startTime - 1;
  if (range.end > range.start) {
    timeRanges->insert(timeRanges->begin() + i + 1, range);
  }
  if (timeRange.end <= timeRange.start) {
    timeRanges->erase(timeRanges->begin() + i);
  } else {
    (*timeRanges)[i] = timeRange;
  }
}
static void TargetIntersectTimeRange(std::vector<TimeRange>* timeRanges, int i, Frame startTime,
                                     Frame endTime) {
  auto timeRange = (*timeRanges)[i];
  if (timeRange.start < startTime) {
    timeRange.end = startTime - 1;
  } else {
    timeRange.start = endTime + 1;
  }
  if (timeRange.end <= timeRange.start) {
    timeRanges->erase(timeRanges->begin() + i);
  } else {
    (*timeRanges)[i] = timeRange;
  }
}

void SubtractFromTimeRanges(std::vector<TimeRange>* timeRanges, Frame startTime, Frame endTime) {
  if (endTime < startTime) {
    SplitTimeRangesAt(timeRanges, startTime);
    return;
  }
  auto size = static_cast<int>(timeRanges->size());
  for (int i = size - 1; i >= 0; i--) {
    auto timeRange = (*timeRanges)[i];
    if (timeRange.end < startTime || timeRange.start > endTime) {
      // target outside timeRange
      continue;
    }
    if (timeRange.start < startTime && timeRange.end > endTime) {
      // target inside timeRange
      TargetInsideTimeRange(timeRanges, i, startTime, endTime);
      break;
    }
    if (timeRange.start >= startTime && timeRange.end <= endTime) {
      // target contain timeRange
      timeRanges->erase(timeRanges->begin() + i);
    } else {
      // target intersect timeRange
      TargetIntersectTimeRange(timeRanges, i, startTime, endTime);
    }
  }
}

void SplitTimeRangesAt(std::vector<TimeRange>* timeRanges, Frame startTime) {
  auto size = static_cast<int>(timeRanges->size());
  for (int i = size - 1; i >= 0; i--) {
    auto timeRange = (*timeRanges)[i];
    if (timeRange.start == startTime || timeRange.end < startTime) {
      break;
    }
    if (timeRange.start < startTime && timeRange.end >= startTime) {
      TimeRange range = {startTime, timeRange.end};
      (*timeRanges)[i].end = timeRange.end = startTime - 1;
      if (range.end > range.start) {
        timeRanges->insert(timeRanges->begin() + i + 1, range);
      }
      if (timeRange.end <= timeRange.start) {
        timeRanges->erase(timeRanges->begin() + i);
      }
      break;
    }
  }
}

void MergeTimeRanges(std::vector<TimeRange>* timeRanges,
                     const std::vector<TimeRange>* otherRanges) {
  std::vector<TimeRange> intersections;
  auto mergeFunc = [otherRanges, &intersections](TimeRange& timeRange) {
    for (auto range : *otherRanges) {
      if (range.end < timeRange.start) {
        continue;
      }
      if (range.start > timeRange.end) {
        break;
      }
      if (range.start <= timeRange.start && range.end >= timeRange.end) {
        intersections.push_back(timeRange);
        break;
      }
      if (range.start >= timeRange.start && range.end <= timeRange.end) {
        intersections.push_back(range);
        continue;
      }
      if (range.start < timeRange.start) {
        TimeRange newRange = {timeRange.start, range.end};
        intersections.push_back(newRange);
        continue;
      }
      if (range.end > timeRange.end) {
        TimeRange newRange = {range.start, timeRange.end};
        intersections.push_back(newRange);
      }
    }
  };
  std::for_each(timeRanges->begin(), timeRanges->end(), mergeFunc);
  *timeRanges = intersections;
}

std::vector<TimeRange> OffsetTimeRanges(const std::vector<TimeRange>& timeRanges,
                                        Frame offsetTime) {
  std::vector<TimeRange> newTimeRanges = {};
  for (auto timeRange : timeRanges) {
    timeRange.start += offsetTime;
    timeRange.end += offsetTime;
    newTimeRanges.push_back(timeRange);
  }
  return newTimeRanges;
}

bool HasVaryingTimeRange(const std::vector<TimeRange>* staticTimeRanges, Frame startTime,
                         Frame duration) {
  if (staticTimeRanges->size() == 1) {
    const auto& range = *staticTimeRanges->data();
    if (range.start == startTime && range.end == startTime + duration - 1) {
      return false;
    }
  }
  return true;
}

int FindTimeRangeAt(const std::vector<TimeRange>& timeRanges, Frame position, int start, int end) {
  if (start > end) {
    return -1;
  }
  auto index = static_cast<int>((start + end) * 0.5);
  const auto& timeRange = timeRanges[index];
  if (timeRange.start > position) {
    return FindTimeRangeAt(timeRanges, position, start, index - 1);
  } else if (timeRange.end < position) {
    return FindTimeRangeAt(timeRanges, position, index + 1, end);
  } else {
    return index;
  }
}

TimeRange GetTimeRangeContains(const std::vector<TimeRange>& timeRanges, Frame frame) {
  auto index = FindTimeRangeAt(timeRanges, frame, 0, static_cast<int>(timeRanges.size() - 1));
  if (index != -1) {
    return timeRanges[index];
  }
  return {frame, frame};
}

Frame ConvertFrameByStaticTimeRanges(const std::vector<TimeRange>& timeRanges, Frame frame) {
  auto index = FindTimeRangeAt(timeRanges, frame, 0, static_cast<int>(timeRanges.size() - 1));
  return index != -1 ? timeRanges[index].start : frame;
}
}  // namespace pag
