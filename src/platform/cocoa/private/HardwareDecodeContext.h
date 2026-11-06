/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include <CoreVideo/CoreVideo.h>
#include <mutex>

namespace pag {
/**
 * Carries the pixel buffer produced by one VideoToolbox decode call. VideoToolbox may invoke the
 * output callback on another thread and does not promise that it has returned by the time
 * VTDecompressionSessionDecodeFrame returns, so the buffer cannot be written into a stack slot of
 * the decoding frame. The decode thread and the callback each complete this context once, and
 * whichever finishes last releases it, which keeps the storage and the buffer valid until both
 * sides are done.
 */
class HardwareDecodeContext {
 public:
  ~HardwareDecodeContext() {
    if (outputPixelBuffer != nullptr) {
      CVPixelBufferRelease(outputPixelBuffer);
    }
  }

  /**
   * Takes over the pixel buffer delivered by the VideoToolbox output callback. The buffer is
   * released instead when the decode thread has already finished without waiting for it.
   */
  void complete(CVPixelBufferRef buffer) {
    bool shouldDelete = false;
    {
      std::lock_guard<std::mutex> lock(locker);
      callbackDone = true;
      if (consumerDone) {
        shouldDelete = !abandoned;
      } else {
        outputPixelBuffer = buffer;
        buffer = nullptr;
      }
    }
    if (buffer != nullptr) {
      CVPixelBufferRelease(buffer);
    }
    if (shouldDelete) {
      delete this;
    }
  }

  /**
   * Called by the decode thread once VTDecompressionSessionDecodeFrame returns, the returned buffer
   * is owned by the caller. callbackExpected is false when the call failed, in which case
   * VideoToolbox promises no callback, but a delivered one would still write here, so the context
   * keeps itself alive and reports the leak through abandonedOut.
   */
  CVPixelBufferRef takeOutput(bool callbackExpected, bool* abandonedOut) {
    CVPixelBufferRef result = nullptr;
    bool abandonedNow = false;
    bool shouldDelete = false;
    {
      std::lock_guard<std::mutex> lock(locker);
      consumerDone = true;
      result = outputPixelBuffer;
      outputPixelBuffer = nullptr;
      if (callbackDone) {
        shouldDelete = true;
      } else if (!callbackExpected) {
        abandoned = true;
      }
      abandonedNow = abandoned;
    }
    if (abandonedOut != nullptr) {
      *abandonedOut = abandonedNow;
    }
    if (shouldDelete) {
      delete this;
    }
    return result;
  }

 private:
  std::mutex locker = {};
  CVPixelBufferRef outputPixelBuffer = nullptr;
  bool callbackDone = false;
  bool consumerDone = false;
  bool abandoned = false;
};
}  // namespace pag
