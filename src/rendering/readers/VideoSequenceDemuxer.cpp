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
#include <list>
#include "base/utils/TimeUtil.h"

namespace pag {
VideoSequenceDemuxer::VideoSequenceDemuxer(VideoSequence* sequence) : sequence(sequence) {
}

void VideoSequenceDemuxer::seekTo(int64_t timeUs) {
  if (timeUs < 0) {
    return;
  }
  auto keyframeIndex = ptsDetail()->findKeyframeIndex(timeUs);
  seekFrameIndex = ptsDetail()->keyframeIndexVector[keyframeIndex];
}

int64_t VideoSequenceDemuxer::getSampleTime() {
  auto totalFrames = static_cast<int>(sequence->frames.size());
  if (currentFrameIndex >= totalFrames) {
    return INT64_MAX;
  }
  return FrameToTime(sequence->frames[currentFrameIndex]->frame, sequence->frameRate);
}

bool VideoSequenceDemuxer::advance() {
  if (seekFrameIndex >= 0) {
    currentFrameIndex = seekFrameIndex;
    seekFrameIndex = INT_MIN;
  } else {
    if (currentFrameIndex >= static_cast<int>(sequence->frames.size())) {
      return false;
    }
    ++currentFrameIndex;
  }
  if (currentFrameIndex < static_cast<int>(sequence->frames.size())) {
    afterAdvance(sequence->frames[currentFrameIndex]->isKeyframe);
  }
  return true;
}

SampleData VideoSequenceDemuxer::readSampleData() {
  if (currentFrameIndex >= static_cast<int>(sequence->frames.size())) {
    return {};
  }
  auto data = sequence->frames[currentFrameIndex]->fileBytes;
  SampleData sampleData = {};
  sampleData.data = data->data();
  sampleData.length = data->length();
  return sampleData;
}

std::shared_ptr<PTSDetail> VideoSequenceDemuxer::createPTSDetail() {
  std::vector<int> keyframeIndexVector{};
  auto totalFrames = static_cast<int>(sequence->frames.size());
  std::list<int64_t> ptsList{};
  for (int i = totalFrames - 1; i >= 0; --i) {
    auto videoFrame = sequence->frames[i];
    auto pts = FrameToTime(videoFrame->frame, sequence->frameRate);
    int index = 0;
    auto it = ptsList.begin();
    for (; it != ptsList.end(); ++it) {
      if (*it < pts) {
        ++index;
        continue;
      }
      break;
    }
    ptsList.insert(it, pts);
    if (videoFrame->isKeyframe) {
      index = totalFrames - (static_cast<int>(ptsList.size()) - index);
      keyframeIndexVector.insert(keyframeIndexVector.begin(), index);
    }
  }
  auto duration = FrameToTime(sequence->duration(), sequence->frameRate);
  auto ptsVector = std::vector<int64_t>{std::make_move_iterator(ptsList.begin()),
                                        std::make_move_iterator(ptsList.end())};
  return std::make_shared<PTSDetail>(duration, std::move(ptsVector),
                                     std::move(keyframeIndexVector));
}
}  // namespace pag
