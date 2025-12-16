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

#include "MovieInfo.h"
#include "MovieDemuxer.h"
#include "base/utils/TimeUtil.h"
#include "platform/Platform.h"
#include "rendering/sequences/VideoReader.h"

namespace pag {

static tgfx::Orientation RotationToOrientation(int rotation) {
  int r = std::abs(rotation / 90);
  tgfx::Orientation orientation = tgfx::Orientation::TopLeft;
  switch (r) {
    case 1:
      orientation = tgfx::Orientation::RightTop;
      break;
    case 2:
      orientation = tgfx::Orientation::BottomRight;
      break;
    case 3:
      orientation = tgfx::Orientation::LeftBottom;
      break;
    default:
      orientation = tgfx::Orientation::TopLeft;
      break;
  }
  return orientation;
}

std::shared_ptr<MovieInfo> MovieInfo::Load(const std::string& filePath, int64_t startTime,
                                           int64_t duration, float speed) {
  auto info = std::shared_ptr<MovieInfo>(new MovieInfo(filePath));
  if (info->demuxer == nullptr) {
    return nullptr;
  }
  auto format = info->demuxer->getFormat();
  if (startTime > format.duration || format.width <= 0 || format.height <= 0) {
    return nullptr;
  }
  info->weakThis = info;
  info->speed = speed;
  info->startTime = std::max(static_cast<int64_t>(0), startTime);
  info->frameRate = format.frameRate;
  info->videoWidth = format.width;
  info->videoHeight = format.height;

  auto endTime = ((startTime == -1 && duration == -1) ? format.duration : (startTime + duration));
  info->durationMs = std::min(endTime, format.duration) - info->startTime;
  info->durationMs =
      static_cast<int64_t>((speed == 0 ? 0 : static_cast<double>(info->durationMs) / speed));
  return info;
}

std::shared_ptr<SequenceReader> MovieInfo::makeReader(std::shared_ptr<File>, PAGFile*, bool) {
  if (demuxer == nullptr) {
    demuxer = MovieDemuxer::Make(filePath, Platform::Current()->naluType());
  }
  if (demuxer == nullptr) {
    return nullptr;
  }
  demuxer->selectTrack(trackID);
  return std::make_shared<VideoReader>(std::move(demuxer));
}

std::shared_ptr<tgfx::Image> MovieInfo::makeStaticImage(std::shared_ptr<File> file, bool) {
  if (file == nullptr || !staticContent()) {
    return nullptr;
  }
  auto generator = std::make_shared<StaticSequenceGenerator>(std::move(file), weakThis.lock(),
                                                             videoWidth, videoHeight, false);
  auto image = tgfx::Image::MakeFrom(std::move(generator));
  if (image != nullptr) {
    image = image->makeOriented(RotationToOrientation(rotation));
  }
  return image;
}

std::shared_ptr<tgfx::Image> MovieInfo::makeFrameImage(std::shared_ptr<SequenceReader> reader,
                                                       Frame targetFrame, bool) {
  if (reader == nullptr) {
    return nullptr;
  }
  auto generator = std::make_shared<SequenceFrameGenerator>(std::move(reader), targetFrame);
  auto image = tgfx::Image::MakeFrom(std::move(generator));
  if (image != nullptr) {
    image = image->makeOriented(RotationToOrientation(rotation));
  }
  return image;
}

ID MovieInfo::uniqueID() const {
  return movieID;
}

int MovieInfo::width() const {
  int r = std::abs(rotation / 90);
  return (r == 1 || r == 3) ? videoHeight : videoWidth;
}

int MovieInfo::height() const {
  int r = std::abs(rotation / 90);
  return (r == 1 || r == 3) ? videoWidth : videoHeight;
}

Frame MovieInfo::duration() const {
  return TimeToFrame(durationMs * speed, frameRate);
}

bool MovieInfo::isVideo() const {
  return true;
}

bool MovieInfo::staticContent() const {
  return duration() <= 1;
}

Frame MovieInfo::firstVisibleFrame(const Layer*) const {
  return TimeToFrame(startTime, frameRate);
}

MovieInfo::MovieInfo(const std::string& filePath) : SequenceInfo(nullptr), filePath(filePath) {
  demuxer = MovieDemuxer::Make(filePath, Platform::Current()->naluType());
  if (demuxer == nullptr) {
    return;
  }
  trackID = demuxer->getTrackID();
  rotation = demuxer->getRotation();
}

void MovieInfo::setMovieID(ID uniqueID) {
  movieID = uniqueID;
}

}  // namespace pag
