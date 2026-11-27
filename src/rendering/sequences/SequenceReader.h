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

#pragma once

#include <atomic>
#include <mutex>
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/Performance.h"
#include "rendering/video/FrameRead.h"
#include "tgfx/core/ImageBuffer.h"

namespace pag {

enum class SequenceReadStatus {
  Pending,
  // Buffer preparation completed. Final delivery must be checked through the associated FrameRead.
  Succeeded,
  Failed,
  // The reader returned a previously decoded buffer for the requested frame because decoding
  // failed. Consumers must not treat the read as a real success (for example, the frame should
  // not be persisted into a disk cache), while the display can still show the stale content.
  Fallback,
};

struct SequenceReadResult {
  uint64_t requestID = 0;
  Frame targetFrame = -1;
  std::atomic<SequenceReadStatus> status = SequenceReadStatus::Pending;

  /** Returns the read constraints and delivery state associated with the decoded buffer. */
  std::shared_ptr<FrameRead> getFrameRead();

  /** Binds an existing buffer's frame read while retaining this request's recorded deadline. */
  void setFrameRead(std::shared_ptr<FrameRead> frame);

  /** Records the shared publication deadline without changing tgfx's native read timeout. */
  void setFrameReadDeadline(int64_t deadline);

 private:
  std::mutex frameReadLocker = {};
  std::shared_ptr<FrameRead> frameRead = std::make_shared<FrameRead>();
};

class SequenceReader {
 public:
  virtual ~SequenceReader() = default;

  /**
   * Returns the width of the sequence buffers created from the reader.
   */
  virtual int width() const = 0;

  /**
   * Returns the height of the sequence buffers created from the reader.
   */
  virtual int height() const = 0;

  /**
   * Prepares a buffer for the target frame immediately. Deferred buffers may still be Pending;
   * consumers must verify the associated FrameRead after texture generation completes.
   */
  std::shared_ptr<tgfx::ImageBuffer> readBuffer(
      Frame targetFrame, const std::shared_ptr<SequenceReadResult>& result = nullptr);

  void reportPerformance(Performance* performance);

 protected:
  /**
   * Return the decoded ImageBuffer of the specified frame. Implementations that synthesize a
   * buffer for the requested frame (instead of really decoding it) should store the
   * SequenceReadStatus::Fallback status into the result, which is otherwise filled in by
   * readBuffer().
   */
  virtual std::shared_ptr<tgfx::ImageBuffer> onMakeBuffer(
      Frame targetFrame, const std::shared_ptr<SequenceReadResult>& result) = 0;

  /**
   * Reports the decoding performance data.
   */
  virtual void onReportPerformance(Performance* performance, int64_t decodingTime) = 0;

 private:
  std::atomic_int64_t decodingTime = 0;
};
}  // namespace pag
