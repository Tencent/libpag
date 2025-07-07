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

#include "pag/file.h"

namespace pag {

inline Frame TimeToFrame(int64_t time, double frameRate) {
  return static_cast<Frame>(floor(time * frameRate / 1000000.0));
}

inline int64_t FrameToTime(Frame frame, double frameRate) {
  return static_cast<Frame>(ceil(frame * 1000000.0 / frameRate));
}

inline double ClampProgress(double progress) {
  auto percent = fmod(progress, 1.0);
  if (percent <= 0 && progress != 0) {
    percent += 1.0;
  }
  return percent;
}

inline Frame ProgressToFrame(double progress, Frame totalFrames) {
  if (totalFrames <= 1) {
    return 0;
  }
  auto percent = ClampProgress(progress);
  // 'progress' ranges in [0, 1], but 'frame' ranges in [frame, frame+1), so the last frame needs
  // special handling.
  auto currentFrame = static_cast<Frame>(floor(percent * static_cast<double>(totalFrames)));
  return currentFrame == totalFrames ? totalFrames - 1 : currentFrame;
}

inline double FrameToProgress(Frame currentFrame, Frame totalFrames) {
  if (totalFrames <= 1 || currentFrame < 0) {
    return 0;
  }
  if (currentFrame >= totalFrames - 1) {
    return 1;
  }
  // todo(partyhuang): figure out which one is best for offsetting, 0.1 or 0.5?
  return (currentFrame * 1.0 + 0.1) / totalFrames;
}

inline int64_t ProgressToTime(double progress, int64_t totalTime) {
  if (totalTime <= 1) {
    return 0;
  }
  auto percent = ClampProgress(progress);
  auto currentTime = static_cast<int64_t>(floor(percent * static_cast<double>(totalTime)));
  return currentTime == totalTime ? currentTime - 1 : currentTime;
}

inline double TimeToProgress(int64_t currentTime, int64_t totalTime) {
  if (totalTime <= 1 || currentTime < 0) {
    return 0;
  }
  if (currentTime >= totalTime - 1) {
    return 1;
  }
  return static_cast<double>(currentTime) / static_cast<double>(totalTime);
}
}  // namespace pag
