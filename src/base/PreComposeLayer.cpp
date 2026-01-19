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
std::unique_ptr<PreComposeLayer> PreComposeLayer::Wrap(pag::Composition* composition) {
  auto layer = new PreComposeLayer();
  layer->duration = composition->duration;
  layer->transform = Transform2D::MakeDefault().release();
  layer->composition = composition;
  return std::unique_ptr<PreComposeLayer>(layer);
}

void PreComposeLayer::excludeVaryingRanges(std::vector<TimeRange>* timeRanges) {
  Layer::excludeVaryingRanges(timeRanges);
  if (timeRanges->empty()) {
    return;
  }
  auto ranges = getContentStaticTimeRanges();
  TimeRange startRange = {0, startTime - 1};
  if (startRange.start < startRange.end) {
    ranges.insert(ranges.begin(), startRange);
  }
  auto endTime = timeRanges->back().end;
  TimeRange endRange = {startTime + duration, endTime};
  if (endRange.start < endRange.end) {
    ranges.push_back(endRange);
  }
  MergeTimeRanges(timeRanges, &ranges);
}

std::vector<TimeRange> PreComposeLayer::getContentStaticTimeRanges() const {
  auto ranges = composition->staticTimeRanges;
  float timeScale = 1;
  if (containingComposition) {
    timeScale = containingComposition->frameRate / composition->frameRate;
  }
  for (auto i = static_cast<int>(ranges.size() - 1); i >= 0; i--) {
    auto& range = ranges[i];
    range.start = static_cast<Frame>(roundf(range.start * timeScale));
    range.end = static_cast<Frame>(roundf(range.end * timeScale));
    range.start += compositionStartTime;
    range.end += compositionStartTime;
    if (range.end <= startTime || range.start >= startTime + duration - 1) {
      ranges.erase(ranges.begin() + i);
    } else {
      if (range.start < startTime) {
        range.start = startTime;
      }
      if (range.end > startTime + duration - 1) {
        range.end = startTime + duration - 1;
      }
      if (range.start == range.end) {
        ranges.erase(ranges.begin() + i);
      }
    }
  }
  return ranges;
}

Frame PreComposeLayer::getCompositionFrame(Frame layerFrame) {
  auto timeScale =
      containingComposition ? (composition->frameRate / containingComposition->frameRate) : 1.0f;
  return static_cast<Frame>(
      roundf(static_cast<float>(layerFrame - compositionStartTime) * timeScale));
}

bool PreComposeLayer::verify() const {
  if (!Layer::verify()) {
    VerifyFailed();
    return false;
  }
  VerifyAndReturn(composition != nullptr);
}

Rect PreComposeLayer::getBounds() const {
  return Rect::MakeWH(composition->width, composition->height);
}
}  // namespace pag
