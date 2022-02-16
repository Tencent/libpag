/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "VideoBuffer.h"
#include "base/utils/Log.h"
#include "codec/NALUType.h"
#include "pag/decoder.h"
#include "pag/file.h"

namespace pag {
enum class DecodingResult {
  Success = 0,
  TryAgainLater = -1,
  Error = -2,
  EndOfStream = -3,
};

struct VideoConfig {
  int width = 0;
  int height = 0;
  float frameRate = 0.0;
  bool hasAlpha = false;
  std::vector<std::shared_ptr<ByteData>> headers = {};
  tgfx::YUVColorSpace colorSpace = tgfx::YUVColorSpace::Rec601;
  tgfx::YUVColorRange colorRange = tgfx::YUVColorRange::MPEG;
  std::string mimeType = "video/avc";
};

class VideoDecoder {
 public:
  /**
   * Returns true if hardware video decoders are supported on current platform.
   */
  static bool HasHardwareDecoder();

  /**
   * Returns true if software video decoders are supported on current platform.
   */
  static bool HasSoftwareDecoder();

  /**
   * Returns true if a external software decoder is registered. We assume that it has better
   * performance than libavc.
   */
  static bool HasExternalSoftwareDecoder();

  /**
   * Returns the registered SoftwareDecoderFactory.
   */
  static SoftwareDecoderFactory* GetExternalSoftwareDecoderFactory();

  /**
   * Returns the maximum number of hardware video decoders we can create.
   */
  static int GetMaxHardwareDecoderCount();

  /**
   * If true, VideoDecoder uses a software decoder first, but initializes a hardware on
   * async thread, and then switches to the hardware decoder when it is initialized.
   * The default value is true, which will improve the performance of first frame rendering.
   */
  static bool SoftwareToHardwareEnabled();

  /**
   * Creates a new video decoder by specified type. Returns a hardware video decoder if useHardware
   * is true, otherwise, returns a software video decoder.
   */
  static std::unique_ptr<VideoDecoder> Make(const VideoConfig& config, bool useHardware);

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
  virtual std::shared_ptr<VideoBuffer> onRenderFrame() = 0;

  /**
   * Returns current presentation time.
   */
  virtual int64_t presentationTime() = 0;

 private:
  bool hardwareBacked = false;

  static std::unique_ptr<VideoDecoder> CreateHardwareDecoder(const VideoConfig& config);
  static std::unique_ptr<VideoDecoder> CreateSoftwareDecoder(const VideoConfig& config);
};
}  // namespace pag
