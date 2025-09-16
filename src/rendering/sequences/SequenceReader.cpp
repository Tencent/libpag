/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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
#include "rendering/caches/RenderCache.h"
#include "rendering/sequences/BitmapSequenceReader.h"
#include "rendering/sequences/VideoReader.h"
#include "rendering/sequences/VideoSequenceDemuxer.h"

namespace pag {
std::shared_ptr<tgfx::ImageBuffer> SequenceReader::readBuffer(Frame targetFrame) {
  tgfx::Clock clock = {};
  auto buffer = onMakeBuffer(targetFrame);
  decodingTime += clock.measure();
  return buffer;
}

void SequenceReader::reportPerformance(Performance* performance) {
  if (decodingTime > 0) {
    onReportPerformance(performance, decodingTime);
    decodingTime = 0;
  }
}
}  // namespace pag
