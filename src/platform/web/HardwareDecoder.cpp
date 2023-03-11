/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "HardwareDecoder.h"
#include "base/utils/TimeUtil.h"
#include "codec/mp4/MP4BoxHelper.h"
#include "rendering/sequences/VideoSequenceDemuxer.h"
#include "tgfx/opengl/GLFunctions.h"

using namespace emscripten;

namespace pag {
HardwareDecoder::HardwareDecoder(const VideoFormat& format) {
  auto demuxer = static_cast<VideoSequenceDemuxer*>(format.demuxer);
  file = demuxer->file;
  rootFile = demuxer->pagFile;
  auto sequence = demuxer->sequence;
  _width = sequence->getVideoWidth();
  _height = sequence->getVideoHeight();
  frameRate = sequence->frameRate;
  auto staticTimeRanges = val::array();
  for (const auto& timeRange : sequence->staticTimeRanges) {
    auto object = val::object();
    object.set("start", static_cast<int>(timeRange.start));
    object.set("end", static_cast<int>(timeRange.end));
    staticTimeRanges.call<void>("push", object);
  }
  val isIPhone = val::module_property("isIPhone");
  if (isIPhone().as<bool>()) {
    auto videoSequence = *sequence;
    videoSequence.MP4Header = nullptr;
    std::vector<VideoFrame> videoFrames;
    for (auto& frame : sequence->frames) {
      if (!videoFrames.empty() && frame->isKeyframe) {
        break;
      }
      videoFrames.push_back(*frame);
      auto& videoFrame = videoFrames.back();
      videoFrame.frame += static_cast<Frame>(sequence->frames.size());
      videoSequence.frames.emplace_back(&videoFrame);
    }
    mp4Data = MP4BoxHelper::CovertToMP4(&videoSequence);
    videoSequence.frames.clear();
    videoSequence.headers.clear();
    for (auto& frame : videoFrames) {
      frame.fileBytes = nullptr;
    }
  } else {
    mp4Data = MP4BoxHelper::CovertToMP4(sequence);
  }
  auto videoReaderClass = val::module_property("VideoReader");
  videoReader = videoReaderClass
                    .call<val>("create", val(typed_memory_view(mp4Data->length(), mp4Data->data())),
                               _width, _height, sequence->frameRate, staticTimeRanges)
                    .await();
  auto video = videoReader.call<val>("getVideo");
  if (!video.isNull()) {
    videoImageReader = tgfx::VideoImageReader::MakeFrom(video, _width, _height);
  }
}

HardwareDecoder::~HardwareDecoder() {
  if (videoReader.as<bool>()) {
    videoReader.call<void>("onDestroy");
  }
}

DecodingResult HardwareDecoder::onSendBytes(void*, size_t, int64_t time) {
  pendingTimeStamp = time;
  return DecodingResult::Success;
}

DecodingResult HardwareDecoder::onEndOfStream() {
  endOfStream = true;
  return DecodingResult::Success;
}

DecodingResult HardwareDecoder::onDecodeFrame() {
  if (endOfStream) {
    return DecodingResult::EndOfStream;
  }
  currentTimeStamp = pendingTimeStamp;
  return DecodingResult::Success;
}

void HardwareDecoder::onFlush() {
  endOfStream = false;
}

int64_t HardwareDecoder::presentationTime() {
  return currentTimeStamp;
}

std::shared_ptr<tgfx::ImageBuffer> HardwareDecoder::onRenderFrame() {
  float playbackRate = 1;
  if (rootFile != nullptr && rootFile->timeStretchMode() == PAGTimeStretchMode::Scale) {
    playbackRate = file->duration() / ((rootFile->duration() / 1000000) * rootFile->frameRate());
  }
  auto targetFrame = TimeToFrame(currentTimeStamp, frameRate);
  val promise = videoReader.call<val>("prepare", static_cast<int>(targetFrame), playbackRate);
  auto imageBuffer = videoImageReader->acquireNextBuffer(promise);
  return imageBuffer;
}
}  // namespace pag
