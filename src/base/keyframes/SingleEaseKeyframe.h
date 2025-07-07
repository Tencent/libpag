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

#pragma once

#include "base/utils/BezierEasing.h"
#include "pag/file.h"

namespace pag {
template <typename T>
class SingleEaseKeyframe : public Keyframe<T> {
 public:
  ~SingleEaseKeyframe() override {
    delete interpolator;
  }

  void initialize() override {
    if (this->interpolationType == KeyframeInterpolationType::Bezier) {
      interpolator = new BezierEasing(this->bezierOut[0], this->bezierIn[0]);
    } else {
      interpolator = new Interpolator();
    }
  }

  float getProgress(Frame time) {
    auto progress = static_cast<float>(time - this->startTime) / (this->endTime - this->startTime);
    return interpolator->getInterpolation(progress);
  }

  T getValueAt(Frame time) override {
    auto progress = getProgress(time);
    return Interpolate(this->startValue, this->endValue, progress);
  }

 private:
  Interpolator* interpolator = nullptr;
};
}  // namespace pag
