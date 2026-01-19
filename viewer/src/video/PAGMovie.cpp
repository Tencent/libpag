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

#include "PAGMovie.h"
#include "audio/model/AudioSource.h"
#include "base/utils/TimeUtil.h"
#include "rendering/graphics/Picture.h"
#include "rendering/sequences/SequenceImageProxy.h"

namespace pag {

std::shared_ptr<PAGMovie> PAGMovie::MakeFromFile(const std::string& filePath) {
  return MakeFromFile(filePath, -1, -1);
}

std::shared_ptr<PAGMovie> PAGMovie::MakeFromFile(const std::string& filePath, int64_t startTime,
                                                 int64_t duration, float speed) {
  if (filePath.empty() || speed < 0) {
    return nullptr;
  }
  auto movieInfo = MovieInfo::Load(filePath, startTime, duration, speed);
  if (movieInfo == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGMovie>(new PAGMovie(movieInfo));
}

PAGMovie::PAGMovie(std::shared_ptr<MovieInfo> info)
    : PAGImage(info->width(), info->height()), movieInfo(std::move(info)) {
  movieInfo->setMovieID(uniqueID());
}

int64_t PAGMovie::duration() {
  return movieInfo->durationMs;
}

std::vector<AudioClip> PAGMovie::generateAudioClips() {
  auto speed = movieInfo->speed;
  if (speed <= 0 || movieInfo->duration() <= 1) {
    return {};
  }
  auto startTime = movieInfo->startTime;
  auto duration = movieInfo->durationMs;
  auto sourceTimeRange = TimeRange{startTime, startTime + static_cast<int64_t>(duration * speed)};
  auto targetTimeRange = TimeRange{0, duration};
  AudioClip clip = {std::make_shared<AudioSource>(movieInfo->filePath), sourceTimeRange,
                    targetTimeRange};
  return {clip};
}

Frame PAGMovie::getContentFrame(int64_t time) const {
  if (movieInfo->speed <= 0 || movieInfo->duration() <= 1) {
    return TimeToFrame(movieInfo->startTime, movieInfo->frameRate);
  }
  time = std::max(static_cast<int64_t>(0), std::min(time, movieInfo->durationMs));
  time = static_cast<int64_t>(time * (movieInfo->speed)) + movieInfo->startTime;
  auto contentFrame = TimeToFrame(time, movieInfo->frameRate);
  auto startFrame = TimeToFrame(movieInfo->startTime, movieInfo->frameRate);
  auto endFrame = movieInfo->duration() + startFrame - 1;
  return std::max(startFrame, std::min(contentFrame, endFrame));
}

std::shared_ptr<Graphic> PAGMovie::getGraphic(Frame contentFrame) const {
  auto startFrame = TimeToFrame(movieInfo->startTime, movieInfo->frameRate);
  auto endFrame = movieInfo->duration() + startFrame - 1;
  if (startFrame > contentFrame) {
    contentFrame = startFrame;
  } else if (contentFrame > endFrame) {
    contentFrame = endFrame;
  }
  auto proxy = std::make_shared<SequenceImageProxy>(movieInfo, contentFrame);
  return Picture::MakeFrom(uniqueID(), std::move(proxy));
}

bool PAGMovie::isStill() const {
  return false;
}

}  // namespace pag
