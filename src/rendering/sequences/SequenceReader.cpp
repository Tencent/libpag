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

#include "SequenceReader.h"
#include "SequenceReaderFactory.h"
#include "rendering/sequences/VideoSequenceDemuxer.h"

namespace pag {
std::shared_ptr<tgfx::ImageBuffer> SequenceReader::makeBuffer(Frame targetFrame) {
  auto success = decodeFrame(targetFrame);
  if (!success) {
    return nullptr;
  }
  return onMakeBuffer();
}

std::shared_ptr<tgfx::ImageBuffer> SequenceReader::onMakeBuffer() {
  return nullptr;
}

std::shared_ptr<tgfx::Texture> SequenceReader::makeTexture(tgfx::Context* context,
                                                           Frame targetFrame) {
  if (staticContent) {
    targetFrame = 0;
  }
  if (lastFrame == targetFrame) {
    return lastTexture;
  }
  tgfx::Clock clock = {};
  auto success = decodeFrame(targetFrame);
  decodingTime += clock.measure();
  // Release the last texture for immediately reusing.
  lastTexture = nullptr;
  lastFrame = -1;
  if (success) {
    clock.reset();
    lastTexture = onMakeTexture(context);
    lastFrame = targetFrame;
    textureUploadingTime += clock.measure();
  }
  return lastTexture;
}

void SequenceReader::reportPerformance(Performance* performance) {
  if (textureUploadingTime > 0) {
    performance->textureUploadingTime += textureUploadingTime;
    textureUploadingTime = 0;
  }
  if (decodingTime > 0) {
    onReportPerformance(performance, decodingTime);
    decodingTime = 0;
  }
}
}  // namespace pag
