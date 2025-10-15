/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "WebVideoSequenceDemuxer.h"
#include "base/utils/TimeUtil.h"
#include "codec/mp4/MP4BoxHelper.h"

namespace pag {

VideoSample WebVideoSequenceDemuxer::nextSample() {
  if (!hardwareBacked) {
    return VideoSequenceDemuxer::nextSample();
  }
  VideoSample sample = {};
  sample.time = FrameToTime(sampleIndex, sequence->frameRate);
  sample.length = 1;
  sampleIndex++;
  return sample;
}

void WebVideoSequenceDemuxer::seekTo(int64_t targetTime) {
  if (!hardwareBacked) {
    VideoSequenceDemuxer::seekTo(targetTime);
  } else {
    auto targetFrame = TimeToFrame(targetTime, sequence->frameRate);
    maxPTSFrame = sampleIndex = targetFrame;
  }
}

int64_t WebVideoSequenceDemuxer::getSampleTimeAt(int64_t targetTime) {
  if (hardwareBacked) {
    return targetTime;
  }
  return VideoSequenceDemuxer::getSampleTimeAt(targetTime);
}

bool WebVideoSequenceDemuxer::needSeeking(int64_t currentTime, int64_t targetTime) {
  if (hardwareBacked) {
    return true;
  }
  return VideoSequenceDemuxer::needSeeking(currentTime, targetTime);
}

void WebVideoSequenceDemuxer::reset() {
  if (!hardwareBacked) {
    VideoSequenceDemuxer::reset();
  }
}

val WebVideoSequenceDemuxer::getStaticTimeRanges() {
  auto staticTimeRanges = val::array();
  for (const auto& timeRange : sequence->staticTimeRanges) {
    auto object = val::object();
    object.set("start", static_cast<int>(timeRange.start));
    object.set("end", static_cast<int>(timeRange.end));
    staticTimeRanges.call<void>("push", object);
  }
  return staticTimeRanges;
}

std::unique_ptr<ByteData> WebVideoSequenceDemuxer::getMp4Data() {
  val isIPhone = val::module_property("isIPhone");
  std::unique_ptr<ByteData> mp4Data;
  if (isIPhone().as<bool>()) {
    auto videoSequence = *sequence;
    videoSequence.MP4Header = nullptr;
    std::vector<std::shared_ptr<VideoFrame>> videoFrames;
    for (auto& frame : sequence->frames) {
      if (!videoFrames.empty() && frame->isKeyframe) {
        break;
      }
      auto videoFrame = std::make_shared<VideoFrame>(*frame);
      videoFrame->frame += static_cast<Frame>(sequence->frames.size());
      videoSequence.frames.push_back(videoFrame.get());
      videoFrames.push_back(videoFrame);
    }
    mp4Data = MP4BoxHelper::CovertToMP4(&videoSequence);
    videoSequence.frames.clear();
    videoSequence.headers.clear();
    for (auto& frame : videoFrames) {
      frame->fileBytes = nullptr;
    }
  } else {
    mp4Data = MP4BoxHelper::CovertToMP4(sequence);
  }
  return mp4Data;
}

}  // namespace pag
