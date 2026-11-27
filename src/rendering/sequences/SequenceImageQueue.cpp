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
static bool CanReuseSequenceRead(const std::shared_ptr<SequenceReadResult>& result) {
  if (result == nullptr) {
    return false;
  }
  auto status = result->status.load(std::memory_order_acquire);
  if (status == SequenceReadStatus::Failed || status == SequenceReadStatus::Fallback) {
    return false;
  }
  auto frame = result->getFrameRead();
  if (!frame->isCurrent() ||
      (frame->buffer != nullptr &&
       (frame->buffer->expired() || frame->buffer->status() == tgfx::ImageBufferStatus::Expired ||
        frame->buffer->status() == tgfx::ImageBufferStatus::Timeout ||
        frame->buffer->status() == tgfx::ImageBufferStatus::Failed))) {
    return false;
  }
  if (status == SequenceReadStatus::Pending ||
      (frame->buffer != nullptr && frame->buffer->status() == tgfx::ImageBufferStatus::Pending)) {
    return frame->buffer == nullptr || !frame->buffer->expired();
  }
  return frame->hasValidBuffer() && frame->matchesTarget();
}

static bool HasCachedSequenceDelivery(const std::shared_ptr<SequenceReadResult>& result) {
  if (result == nullptr) {
    return false;
  }
  auto status = result->status.load(std::memory_order_acquire);
  auto frame = result->getFrameRead();
  return (status == SequenceReadStatus::Succeeded || status == SequenceReadStatus::Fallback) &&
         frame->hasValidBuffer() && frame->matchesTarget();
}

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
  if (currentResult != nullptr && !HasCachedSequenceDelivery(currentResult)) {
    return;
  }
  auto nextFrame = lastRequestedFrame + 1;
  if (nextFrame >= totalFrames) {
    nextFrame = firstFrame;
  }
  prepare(nextFrame);
}

void SequenceImageQueue::prepare(Frame targetFrame, std::shared_ptr<SequenceReadResult>* result) {
  if (targetFrame < 0 || targetFrame >= totalFrames) {
    return;
  }
  if (preparedImage != nullptr && !CanReuseSequenceRead(preparedResult)) {
    preparedImage = nullptr;
    preparedFrame = -1;
    preparedResult = nullptr;
  }
  if (preparedImage != nullptr) {
    if (result != nullptr && targetFrame == preparedFrame) {
      *result = preparedResult;
    }
    return;
  }
  auto readResult = std::make_shared<SequenceReadResult>();
  readResult->requestID = ++nextRequestID;
  readResult->targetFrame = targetFrame;
  auto image = sequence->makeFrameImage(reader, targetFrame, useDiskCache, readResult);
  if (image != nullptr) {
    preparedImage = image->makeDecoded();
  }
  if (preparedImage == nullptr) {
    readResult->status.store(SequenceReadStatus::Failed, std::memory_order_release);
  } else {
    preparedFrame = targetFrame;
    preparedResult = readResult;
    lastRequestedFrame = targetFrame;
  }
  if (result != nullptr) {
    *result = readResult;
  }
}

std::shared_ptr<tgfx::Image> SequenceImageQueue::getImage(
    Frame targetFrame, std::shared_ptr<SequenceReadResult>* result) {
  lastRequestedFrame = targetFrame;
  if (targetFrame == currentFrame && currentImage != nullptr &&
      CanReuseSequenceRead(currentResult)) {
    if (result != nullptr) {
      *result = currentResult;
    }
    return currentImage;
  }
  if (targetFrame == preparedFrame && !CanReuseSequenceRead(preparedResult)) {
    preparedImage = nullptr;
    preparedFrame = -1;
    preparedResult = nullptr;
  }
  if (targetFrame == preparedFrame) {
    currentImage = preparedImage;
    currentFrame = preparedFrame;
    currentResult = preparedResult;
    preparedImage = nullptr;
    preparedFrame = -1;
    preparedResult = nullptr;
    if (result != nullptr) {
      *result = currentResult;
    }
    return currentImage;
  }
  auto readResult = std::make_shared<SequenceReadResult>();
  readResult->requestID = ++nextRequestID;
  readResult->targetFrame = targetFrame;
  currentFrame = targetFrame;
  currentImage = nullptr;
  currentResult = readResult;
  auto image = sequence->makeFrameImage(reader, targetFrame, useDiskCache, readResult);
  if (image == nullptr) {
    readResult->status.store(SequenceReadStatus::Failed, std::memory_order_release);
    if (result != nullptr) {
      *result = readResult;
    }
    return nullptr;
  }
  auto decodedImage = image->makeDecoded();
  if (decodedImage == nullptr) {
    readResult->status.store(SequenceReadStatus::Failed, std::memory_order_release);
    if (result != nullptr) {
      *result = readResult;
    }
    return nullptr;
  }
  currentImage = std::move(decodedImage);
  currentFrame = targetFrame;
  currentResult = readResult;
  preparedImage = nullptr;
  preparedFrame = -1;
  preparedResult = nullptr;
  if (result != nullptr) {
    *result = currentResult;
  }
  return currentImage;
}

std::shared_ptr<tgfx::Image> SequenceImageQueue::getCachedImage(
    Frame targetFrame, std::shared_ptr<SequenceReadResult>* result) {
  lastRequestedFrame = targetFrame;
  if (targetFrame == preparedFrame && preparedImage != nullptr &&
      HasCachedSequenceDelivery(preparedResult)) {
    currentImage = preparedImage;
    currentFrame = preparedFrame;
    currentResult = preparedResult;
    preparedImage = nullptr;
    preparedFrame = -1;
    preparedResult = nullptr;
  }
  if (result != nullptr) {
    *result = currentResult;
  }
  return HasCachedSequenceDelivery(currentResult) ? currentImage : nullptr;
}

void SequenceImageQueue::invalidateFailedRequest(uint64_t requestID, Frame targetFrame) {
  if (currentResult != nullptr && currentResult->requestID == requestID &&
      currentResult->targetFrame == targetFrame &&
      currentResult->status.load(std::memory_order_acquire) == SequenceReadStatus::Failed) {
    currentFrame = -1;
    currentImage = nullptr;
    currentResult = nullptr;
  }
  if (preparedResult != nullptr && preparedResult->requestID == requestID &&
      preparedResult->targetFrame == targetFrame &&
      preparedResult->status.load(std::memory_order_acquire) == SequenceReadStatus::Failed) {
    preparedFrame = -1;
    preparedImage = nullptr;
    preparedResult = nullptr;
  }
}

void SequenceImageQueue::reportPerformance(Performance* performance) {
  reader->reportPerformance(performance);
}
}  // namespace pag
