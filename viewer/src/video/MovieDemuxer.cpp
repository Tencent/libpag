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

#include "MovieDemuxer.h"
#include <list>
#include "base/utils/TimeUtil.h"

namespace pag {

std::unique_ptr<MovieDemuxer> MovieDemuxer::Make(const std::string& path, NALUType startCodeType) {
  auto demuxer = ffmovie::FFVideoDemuxer::Make(path, static_cast<ffmovie::NALUType>(startCodeType));
  if (demuxer == nullptr) {
    return nullptr;
  }
  return std::unique_ptr<MovieDemuxer>(new MovieDemuxer(std::move(demuxer)));
}

bool MovieDemuxer::advance() {
  return demuxer->advance();
}

bool MovieDemuxer::needSeeking(int64_t currentTime, int64_t targetTime) {
  return demuxer->needSeeking(currentTime, targetTime);
}

void MovieDemuxer::reset() {
  demuxer->reset();
}

void MovieDemuxer::seekTo(int64_t timestamp) {
  demuxer->seekTo(timestamp);
}

void MovieDemuxer::selectTrack(int trackID) {
  demuxer->selectTrack(trackID);
}

int MovieDemuxer::getTrackID() const {
  return trackID;
}

int MovieDemuxer::getRotation() const {
  return rotation;
}

int MovieDemuxer::getTrackCount() {
  return demuxer->getTrackCount();
}

int64_t MovieDemuxer::getSampleTimeAt(int64_t targetTime) {
  return demuxer->getSampleTimeAt(targetTime);
}

int64_t MovieDemuxer::getSampleTime() {
  return demuxer->getSampleTime();
}

VideoFormat MovieDemuxer::getFormat() {
  for (int i = 0; i < getTrackCount(); ++i) {
    auto mediaFormat = demuxer->getTrackFormat(i);
    if (mediaFormat != nullptr &&
        mediaFormat->getString(KEY_MIME).rfind("video", 0) != std::string::npos) {
      VideoFormat videoFormat = {};
      videoFormat.frameRate = mediaFormat->getFloat(KEY_FRAME_RATE);
      videoFormat.width = mediaFormat->getInteger(KEY_WIDTH);
      videoFormat.height = mediaFormat->getInteger(KEY_HEIGHT);
      videoFormat.duration = mediaFormat->getLong(KEY_DURATION);
      videoFormat.mimeType = mediaFormat->getString(KEY_MIME);
      auto colorSpace = mediaFormat->getString(KEY_COLOR_SPACE);
      auto colorRange = mediaFormat->getString(KEY_COLOR_RANGE);
      if (colorSpace == COLORSPACE_REC2020) {
        videoFormat.colorSpace = tgfx::YUVColorSpace::BT2020_LIMITED;
        if (colorRange == COLORRANGE_JPEG) {
          videoFormat.colorSpace = tgfx::YUVColorSpace::BT2020_FULL;
        }
      } else if (colorSpace == COLORSPACE_REC601) {
        videoFormat.colorSpace = tgfx::YUVColorSpace::BT601_LIMITED;
        if (colorRange == COLORRANGE_JPEG) {
          videoFormat.colorSpace = tgfx::YUVColorSpace::BT601_FULL;
        }
      } else if (colorSpace == COLORSPACE_REC709) {
        videoFormat.colorSpace = tgfx::YUVColorSpace::BT709_LIMITED;
        if (colorRange == COLORRANGE_JPEG) {
          videoFormat.colorSpace = tgfx::YUVColorSpace::BT709_FULL;
        }
      }
      for (const auto& header : mediaFormat->headers()) {
        videoFormat.headers.push_back(tgfx::Data::MakeWithCopy(header->data(), header->length()));
      }
      trackID = mediaFormat->getInteger(KEY_TRACK_ID);
      rotation = mediaFormat->getInteger(KEY_ROTATION);
      videoFormat.maxReorderSize = mediaFormat->getInteger(KEY_VIDEO_MAX_REORDER);
      return videoFormat;
    }
  }
  return {};
}

VideoSample MovieDemuxer::nextSample() {
  if (!advance()) {
    return {};
  }
  auto sampleData = demuxer->readSampleData();
  VideoSample packet = {};
  packet.data = sampleData.data;
  packet.time = getSampleTime();
  packet.length = sampleData.length;
  return packet;
}

MovieDemuxer::MovieDemuxer(std::unique_ptr<ffmovie::FFVideoDemuxer> demuxer)
    : demuxer(std::move(demuxer)) {
}

}  // namespace pag