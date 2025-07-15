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

#include "SequenceImageQueue.h"

namespace pag {
std::unique_ptr<SequenceImageQueue> SequenceImageQueue::MakeFrom(
    std::shared_ptr<SequenceInfo> sequence, PAGLayer* pagLayer, bool useDiskCache) {
  if (sequence == nullptr || pagLayer == nullptr || sequence->staticContent()) {
    return nullptr;
  }
  auto reader = sequence->makeReader(pagLayer->getFile(), pagLayer->rootFile, useDiskCache);
  if (reader == nullptr) {
    return nullptr;
  }
  auto firstFrame = sequence->firstVisibleFrame(pagLayer->getLayer());
  return std::unique_ptr<SequenceImageQueue>(
      new SequenceImageQueue(sequence, std::move(reader), firstFrame, useDiskCache));
}

SequenceImageQueue::SequenceImageQueue(std::shared_ptr<SequenceInfo> sequence,
                                       std::shared_ptr<SequenceReader> reader, Frame firstFrame,
                                       bool useDiskCache)
    : sequence(sequence), reader(std::move(reader)), firstFrame(firstFrame),
      totalFrames(sequence->duration()), useDiskCache(useDiskCache) {
}

void SequenceImageQueue::prepareNextImage() {
  auto nextFrame = currentFrame + 1;
  if (nextFrame >= totalFrames) {
    nextFrame = firstFrame;
  }
  prepare(nextFrame);
}

void SequenceImageQueue::prepare(Frame targetFrame) {
  if (preparedImage != nullptr || targetFrame < 0 || targetFrame >= totalFrames) {
    return;
  }
  auto image = sequence->makeFrameImage(reader, targetFrame, useDiskCache);
  preparedImage = image->makeDecoded();
  preparedFrame = targetFrame;
}

std::shared_ptr<tgfx::Image> SequenceImageQueue::getImage(Frame targetFrame) {
  if (targetFrame == currentFrame) {
    return currentImage;
  }
  if (targetFrame == preparedFrame) {
    currentImage = preparedImage;
    preparedImage = nullptr;
    currentFrame = preparedFrame;
    return currentImage;
  }
  auto image = sequence->makeFrameImage(reader, targetFrame, useDiskCache);
  if (image == nullptr) {
    return nullptr;
  }
  currentImage = image->makeDecoded();
  preparedImage = nullptr;
  currentFrame = targetFrame;
  preparedFrame = targetFrame;
  return currentImage;
}

void SequenceImageQueue::reportPerformance(Performance* performance) {
  reader->reportPerformance(performance);
}
}  // namespace pag
