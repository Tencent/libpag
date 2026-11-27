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
#include <cstdint>
#include <limits>
#include <memory>

namespace pag {
/** Private timestamp namespace for a finite PAG video sequence. A fixed translation preserves
 * reordered presentation timestamps while separating outputs produced before and after flush.
 * The codec timestamp is passed to tgfx for frame verification, never to the playback clock. */
class CodecTimeline {
 public:
  explicit CodecTimeline(int64_t duration)
      : duration(duration), stride(duration > 0 && duration < MaxMicroseconds ? duration + 1 : 0) {
    valid = duration > 0 && duration < MaxMicroseconds;
  }

  bool isValid() const {
    return valid;
  }

  static bool hasImage(bool endOfStream, int32_t size) {
    return !endOfStream || size > 0;
  }

  bool encode(int64_t time, int64_t* encoded) const {
    if (!valid || encoded == nullptr || time < 0 || time > duration ||
        time > MaxMicroseconds - offset) {
      return false;
    }
    *encoded = time + offset;
    return true;
  }

  bool decode(int64_t encoded, int64_t* time) const {
    if (!valid || time == nullptr || encoded < offset || encoded - offset > duration) {
      return false;
    }
    *time = encoded - offset;
    return true;
  }

  bool restart() {
    generation->fetch_add(1, std::memory_order_acq_rel);
    if (!valid || stride > MaxMicroseconds - offset ||
        duration > MaxMicroseconds - offset - stride) {
      valid = false;
      return false;
    }
    offset += stride;
    return true;
  }

  void invalidate() {
    generation->fetch_add(1, std::memory_order_acq_rel);
    valid = false;
  }

  const std::shared_ptr<std::atomic<uint64_t>> generation =
      std::make_shared<std::atomic<uint64_t>>(0);

 private:
  static constexpr int64_t MaxMicroseconds = std::numeric_limits<int64_t>::max() / 1000;
  const int64_t duration;
  const int64_t stride;
  int64_t offset = 0;
  bool valid = false;
};
}  // namespace pag
