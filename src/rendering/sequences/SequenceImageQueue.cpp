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

#include "SequenceImageQueue.h"

namespace pag {
static Frame GetFirstVisibleFrame(Sequence* sequence, const Layer* layer) {
  if (layer->type() != LayerType::PreCompose) {
    return 0;
  }
  auto preComposeLayer = static_cast<const PreComposeLayer*>(layer);
  auto composition = preComposeLayer->composition;
  if (composition->type() != CompositionType::Video &&
      composition->type() != CompositionType::Bitmap) {
    return 0;
  }
  auto timeScale = layer->containingComposition
                       ? (composition->frameRate / layer->containingComposition->frameRate)
                       : 1.0f;
  auto compositionFrame = static_cast<Frame>(roundf(
      static_cast<float>(layer->startTime - preComposeLayer->compositionStartTime) * timeScale));
  if (compositionFrame < 0) {
    compositionFrame = 0;
  }
  return sequence->toSequenceFrame(compositionFrame);
}

std::unique_ptr<SequenceImageQueue> SequenceImageQueue::MakeFrom(Sequence* sequence,
                                                                 PAGLayer* pagLayer) {
  if (sequence == nullptr || pagLayer == nullptr || sequence->composition->staticContent()) {
    return nullptr;
  }
  auto reader = SequenceReader::Make(pagLayer->getFile(), sequence, pagLayer->rootFile);
  if (reader == nullptr) {
    return nullptr;
  }
  auto firstFrame = GetFirstVisibleFrame(sequence, pagLayer->getLayer());
  return std::unique_ptr<SequenceImageQueue>(
      new SequenceImageQueue(sequence, std::move(reader), firstFrame));
}

SequenceImageQueue::SequenceImageQueue(Sequence* sequence, std::shared_ptr<SequenceReader> reader,
                                       Frame firstFrame)
    : sequence(sequence), reader(std::move(reader)), firstFrame(firstFrame),
      totalFrames(sequence->duration()) {
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
  auto image = SequenceImage::MakeFrom(reader, sequence, targetFrame);
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
  auto image = SequenceImage::MakeFrom(reader, sequence, targetFrame);
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
