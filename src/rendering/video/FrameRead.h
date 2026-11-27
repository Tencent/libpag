/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
//  in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <memory>
#include "tgfx/core/ImageBuffer.h"

namespace pag {
struct FrameRead {
  struct Options {
    int64_t targetTimestamp = -1;
    // Request bookkeeping only. acquireNextBuffer(targetTimestamp) does not consume this deadline.
    std::atomic<int64_t> deadline = 0;
  };
  const std::shared_ptr<Options> options = std::make_shared<Options>();
  std::shared_ptr<tgfx::ImageBuffer> buffer = nullptr;
  std::shared_ptr<std::atomic<uint64_t>> sourceGeneration = nullptr;
  uint64_t generation = 0;
  // The target passed to acquireNextBuffer(), usually codec PTS * 1000, not a measured receipt.
  // A Ready buffer verifies this binding in tgfx; it does not redefine the playback sample time.
  int64_t verifiedTargetTimestamp = -1;

  bool isCurrent() const {
    return sourceGeneration == nullptr ||
           sourceGeneration->load(std::memory_order_acquire) == generation;
  }

  /** Returns whether this read retains delivered pixels from the current source generation. */
  bool hasValidBuffer() const {
    return isCurrent() && buffer != nullptr && buffer->status() == tgfx::ImageBufferStatus::Ready &&
           !buffer->expired();
  }

  /** Verifies delivery against the target bound when acquiring this buffer, if known. */
  bool matchesTarget() const {
    if (!isCurrent()) {
      return false;
    }
    if (buffer == nullptr) {
      return options->targetTimestamp < 0 && sourceGeneration == nullptr;
    }
    return hasValidBuffer() &&
           (options->targetTimestamp < 0 || verifiedTargetTimestamp == options->targetTimestamp);
  }
};
}  // namespace pag
