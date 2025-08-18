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

#include "HardwareDecoder.h"
#include <emscripten.h>
#include "WebVideoSequenceDemuxer.h"
#include "base/utils/TimeUtil.h"
#include "rendering/sequences/VideoSequenceDemuxer.h"
#include "tgfx/gpu/opengl/GLFunctions.h"

namespace emscripten {
class val;
}
using namespace emscripten;

namespace pag {
HardwareDecoder::HardwareDecoder(const VideoFormat& format) {
  auto demuxer = static_cast<WebVideoSequenceDemuxer*>(format.demuxer);
  file = demuxer->file;
  rootFile = demuxer->pagFile;
  auto sequence = demuxer->sequence;
  _width = sequence->getVideoWidth();
  _height = sequence->getVideoHeight();
  frameRate = sequence->frameRate;
  auto staticTimeRanges = demuxer->getStaticTimeRanges();
  mp4Data = demuxer->getMp4Data();
  auto videoReaderClass = val::module_property("VideoReader");
  videoReader = videoReaderClass.call<val>(
      "create", val(typed_memory_view(mp4Data->length(), mp4Data->data())), _width, _height,
      sequence->frameRate, staticTimeRanges);
  auto video = videoReader.call<val>("getVideo");
  if (!video.isNull()) {
    imageReader = tgfx::VideoElementReader::MakeFrom(video, _width, _height);
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
  videoReader.call<val>("prepare", static_cast<int>(targetFrame), playbackRate);
  int frameId = videoReader.call<int>("getCurrentFrame");
  if (currentFrame < 0 || currentFrame != frameId) {
    currentFrame = frameId;
    lastDecodedBuffer = imageReader->acquireNextBuffer();
  }
  return lastDecodedBuffer;
}
}  // namespace pag
