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
#include <algorithm>
#include "tgfx/core/Clock.h"

namespace pag {
std::shared_ptr<FrameRead> SequenceReadResult::getFrameRead() {
  std::lock_guard<std::mutex> autoLock(frameReadLocker);
  return frameRead;
}

void SequenceReadResult::setFrameRead(std::shared_ptr<FrameRead> frame) {
  std::lock_guard<std::mutex> autoLock(frameReadLocker);
  if (frame == nullptr || frame == frameRead) {
    return;
  }
  auto deadline = frameRead->options->deadline.load(std::memory_order_acquire);
  auto previousDeadline = frame->options->deadline.load(std::memory_order_acquire);
  frame->options->deadline.store(std::max(deadline, previousDeadline), std::memory_order_release);
  frameRead = std::move(frame);
}

void SequenceReadResult::setFrameReadDeadline(int64_t deadline) {
  std::lock_guard<std::mutex> autoLock(frameReadLocker);
  frameRead->options->deadline.store(deadline, std::memory_order_release);
}

std::shared_ptr<tgfx::ImageBuffer> SequenceReader::readBuffer(
    Frame targetFrame, const std::shared_ptr<SequenceReadResult>& result) {
  tgfx::Clock clock = {};
  auto buffer = onMakeBuffer(targetFrame, result);
  decodingTime += clock.measure();
  if (result != nullptr) {
    auto frame = result->getFrameRead();
    if (buffer != nullptr && frame->buffer == nullptr) {
      auto bufferRead = std::make_shared<FrameRead>();
      bufferRead->buffer = buffer;
      bufferRead->options->targetTimestamp = frame->options->targetTimestamp;
      bufferRead->sourceGeneration = frame->sourceGeneration;
      bufferRead->generation = frame->generation;
      bufferRead->verifiedTargetTimestamp = frame->verifiedTargetTimestamp;
      result->setFrameRead(std::move(bufferRead));
    }
    // Succeeded means buffer preparation completed, not that texture delivery matched the request.
    // An implementation may have already reported a more precise status (such as Fallback).
    auto status = result->status.load(std::memory_order_acquire);
    if (status == SequenceReadStatus::Pending) {
      status = buffer != nullptr ? SequenceReadStatus::Succeeded : SequenceReadStatus::Failed;
      result->status.store(status, std::memory_order_release);
    }
  }
  return buffer;
}

void SequenceReader::reportPerformance(Performance* performance) {
  auto currentDecodingTime = decodingTime.exchange(0);
  if (currentDecodingTime > 0) {
    onReportPerformance(performance, currentDecodingTime);
  }
}
}  // namespace pag
