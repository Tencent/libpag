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

#include <set>
#include "base/utils/Verify.h"
#include "pag/file.h"

namespace pag {
CornerPinEffect::~CornerPinEffect() {
  delete upperLeft;
  delete upperRight;
  delete lowerLeft;
  delete lowerRight;
}

bool CornerPinEffect::visibleAt(Frame) const {
  return true;
}

void CornerPinEffect::transformBounds(Rect* contentBounds, const Point&, Frame layerFrame) const {
  auto upperLeftValue = upperLeft->getValueAt(layerFrame);
  auto upperRightValue = upperRight->getValueAt(layerFrame);
  auto lowerLeftValue = lowerLeft->getValueAt(layerFrame);
  auto lowerRightValue = lowerRight->getValueAt(layerFrame);

  auto left = std::min(std::min(upperLeftValue.x, lowerLeftValue.x),
                       std::min(upperRightValue.x, lowerRightValue.x));
  auto top = std::min(std::min(upperLeftValue.y, lowerLeftValue.y),
                      std::min(upperRightValue.y, lowerRightValue.y));
  auto right = std::max(std::max(upperLeftValue.x, lowerLeftValue.x),
                        std::max(upperRightValue.x, lowerRightValue.x));
  auto bottom = std::max(std::max(upperLeftValue.y, lowerLeftValue.y),
                         std::max(upperRightValue.y, lowerRightValue.y));

  contentBounds->setLTRB(left, top, right, bottom);
}

Point CornerPinEffect::getMaxScaleFactor(const Rect& bounds) const {
  float maxWidth = 0;
  float maxHeight = 0;
  std::set<Frame> frames;
  std::vector<Property<Point>*> properties = {upperLeft, upperRight, lowerLeft, lowerRight};
  for (auto* p : properties) {
    if (p->animatable()) {
      auto* animatableP = static_cast<AnimatableProperty<Point>*>(p);
      for (const auto* keyframe : animatableP->keyframes) {
        frames.insert(keyframe->startTime);
        frames.insert(keyframe->endTime);
      }
    } else {
      frames.insert(0);
    }
  }
  Rect temp = Rect::MakeEmpty();
  for (auto frame : frames) {
    transformBounds(&temp, Point::Zero(), frame);
    if (temp.width() > maxWidth) {
      maxWidth = temp.width();
    }
    if (temp.height() > maxHeight) {
      maxHeight = temp.height();
    }
  }
  Point ret = Point::Make(1.f, 1.f);
  if (maxWidth != 0) {
    ret.x = std::max(1.f, maxWidth / bounds.width());
  }
  if (maxHeight != 0) {
    ret.y = std::max(1.f, maxHeight / bounds.height());
  }
  return ret;
}

void CornerPinEffect::excludeVaryingRanges(std::vector<pag::TimeRange>* timeRanges) const {
  Effect::excludeVaryingRanges(timeRanges);
  upperLeft->excludeVaryingRanges(timeRanges);
  upperRight->excludeVaryingRanges(timeRanges);
  lowerLeft->excludeVaryingRanges(timeRanges);
  lowerRight->excludeVaryingRanges(timeRanges);
}

bool CornerPinEffect::verify() const {
  if (!Effect::verify()) {
    VerifyFailed();
    return false;
  }
  VerifyAndReturn(upperLeft != nullptr && upperRight != nullptr && lowerLeft != nullptr &&
                  lowerRight != nullptr);
}
}  // namespace pag
