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

#include "VideoSequenceDemuxer.h"
#include "base/utils/TimeUtil.h"

namespace pag {
VideoSequenceDemuxer::VideoSequenceDemuxer(std::shared_ptr<File> file, VideoSequence* sequence)
    : file(std::move(file)), sequence(sequence) {
  format.width = sequence->alphaStartX + sequence->width;
  if (format.width % 2 == 1) {
    format.width++;
  }
  format.height = sequence->alphaStartY + sequence->height;
  if (format.height % 2 == 1) {
    format.height++;
  }
  for (auto& header : sequence->headers) {
    auto bytes = ByteData::MakeWithoutCopy(header->data(), header->length());
    format.headers.push_back(std::move(bytes));
  }
  format.mimeType = "video/avc";
  format.colorSpace = tgfx::YUVColorSpace::Rec601;
  format.duration = FrameToTime(sequence->duration(), sequence->frameRate);
  format.frameRate = sequence->frameRate;
  for (auto& frame : sequence->frames) {
    if (frame->isKeyframe) {
      keyframes.push_back(frame->frame);
    }
  }
}

int64_t VideoSequenceDemuxer::getSampleTimeAt(int64_t targetTime) const {
  auto frame = TimeToFrame(targetTime, sequence->frameRate);
  return FrameToTime(frame, sequence->frameRate);
}

SampleData VideoSequenceDemuxer::nextSample() {
  if (sampleIndex >= static_cast<int>(sequence->frames.size())) {
    return {};
  }
  SampleData sample = {};
  auto videoFrame = sequence->frames[sampleIndex];
  sample.data = videoFrame->fileBytes->data();
  sample.length = videoFrame->fileBytes->length();
  sample.time = FrameToTime(videoFrame->frame, sequence->frameRate);
  maxPTSFrame = std::max(maxPTSFrame, videoFrame->frame);
  sampleIndex++;
  return sample;
}

bool VideoSequenceDemuxer::needSeeking(int64_t currentSampleTime, int64_t targetSampleTime) const {
  auto current = TimeToFrame(currentSampleTime, sequence->frameRate);
  auto target = TimeToFrame(targetSampleTime, sequence->frameRate);
  if (target < current) {
    return true;
  }
  if (target <= maxPTSFrame) {
    return false;
  }
  const auto& frames = sequence->frames;
  for (Frame frame = target; frame > current + 1; frame--) {
    // DTS == PTS when the frame is key frame.
    if (frames[frame]->isKeyframe) {
      return true;
    }
  }
  return false;
}

void VideoSequenceDemuxer::seekTo(int64_t targetTime) {
  auto targetFrame = TimeToFrame(targetTime, sequence->frameRate);
  int start = 0;
  int end = static_cast<int>(keyframes.size()) - 1;
  while (start < end - 1) {
    int middle = (start + end) / 2;
    if (keyframes[middle] <= targetFrame) {
      start = middle;
    } else {
      end = middle;
    }
  }
  // DTS == PTS when the frame is key frame.
  maxPTSFrame = sampleIndex = keyframes[start];
}

void VideoSequenceDemuxer::reset() {
  maxPTSFrame = -1;
  sampleIndex = 0;
}
}  // namespace pag
