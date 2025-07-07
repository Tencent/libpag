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

#include "DecodingResult.h"
#include "rendering/video/VideoFormat.h"
#include "tgfx/core/ImageBuffer.h"

namespace pag {
class VideoDecoder {
 public:
  virtual ~VideoDecoder();

  /**
   * Returns true if this is a hardware video decoder.
   */
  bool isHardwareBacked() const {
    return hardwareBacked;
  }

  /**
   * Send a frame of bytes for decoding. The same bytes will be sent next time if it returns
   * DecodingResult::TryAgainLater
   */
  virtual DecodingResult onSendBytes(void* bytes, size_t length, int64_t time) = 0;

  /**
   * Called to notify there is no more frame bytes available.
   */
  virtual DecodingResult onEndOfStream() = 0;

  /**
   * Try to decode a new frame from the pending frames sent by onSendBytes(). More pending
   * frames will be sent by onSendBytes() if it returns DecodingResult::TryAgainLater.
   */
  virtual DecodingResult onDecodeFrame() = 0;

  /**
   * Called when seeking happens to clear all pending frames.
   */
  virtual void onFlush() = 0;

  /**
   * Returns decoded video frame to render.
   */
  virtual std::shared_ptr<tgfx::ImageBuffer> onRenderFrame() = 0;

  /**
   * Returns current presentation time.
   */
  virtual int64_t presentationTime() = 0;

 private:
  bool hardwareBacked = false;

  friend class VideoDecoderFactory;
};
}  // namespace pag
