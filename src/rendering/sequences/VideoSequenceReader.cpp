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

#include "VideoSequenceReader.h"
#include "VideoSequenceDemuxer.h"
#include "base/utils/TimeUtil.h"

namespace pag {
VideoSequenceReader::VideoSequenceReader(std::shared_ptr<File> file, VideoSequence* sequence,
                                         DecodingPolicy policy)
    : file(std::move(file)), sequence(sequence) {
  VideoConfig config = {};
  auto demuxer = std::make_unique<VideoSequenceDemuxer>(sequence);
  config.hasAlpha = sequence->alphaStartX + sequence->alphaStartY > 0;
  config.width = sequence->alphaStartX + sequence->width;
  if (config.width % 2 == 1) {
    config.width++;
  }
  config.height = sequence->alphaStartY + sequence->height;
  if (config.height % 2 == 1) {
    config.height++;
  }
  for (auto& header : sequence->headers) {
    auto bytes = ByteData::MakeWithoutCopy(header->data(), header->length());
    config.headers.push_back(std::move(bytes));
  }
  config.mimeType = "video/avc";
  config.colorSpace = tgfx::YUVColorSpace::Rec601;
  config.frameRate = sequence->frameRate;
  reader = std::make_unique<VideoReader>(config, std::move(demuxer), policy);
}

VideoSequenceReader::~VideoSequenceReader() {
  lastTask = nullptr;
}

int64_t VideoSequenceReader::getNextFrameAt(int64_t targetFrame) {
  auto nextFrame = targetFrame + 1;
  return nextFrame >= sequence->duration() ? -1 : nextFrame;
}

bool VideoSequenceReader::decodeFrame(int64_t targetFrame) {
  if (!reader) {
    return false;
  }
  auto targetTime = FrameToTime(targetFrame, sequence->frameRate);
  lastBuffer = reader->readSample(targetTime);
  return lastBuffer != nullptr;
}

std::shared_ptr<tgfx::Texture> VideoSequenceReader::makeTexture(tgfx::Context* context) {
  if (lastBuffer == nullptr) {
    return nullptr;
  }
  return lastBuffer->makeTexture(context);
}

void VideoSequenceReader::reportPerformance(Performance* performance, int64_t decodingTime) const {
  if (reader != nullptr) {
    reader->reportPerformance(performance, decodingTime);
  }
}
}  // namespace pag
