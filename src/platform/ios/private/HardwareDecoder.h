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

#include <CoreMedia/CoreMedia.h>
#include <VideoToolbox/VideoToolbox.h>
#include <chrono>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "platform/cocoa/private/HardwareDecodeContext.h"
#include "rendering/video/VideoDecoder.h"

namespace pag {
struct DecoderToken;

class HardwareDecoder : public VideoDecoder {
 public:
  ~HardwareDecoder() override;

  pag::DecodingResult onSendBytes(void* bytes, size_t length, int64_t time) override;

  pag::DecodingResult onEndOfStream() override;

  pag::DecodingResult onDecodeFrame() override;

  void onFlush() override;

  int64_t presentationTime() override;

  std::shared_ptr<tgfx::ImageBuffer> onRenderFrame() override;

  /**
   * Invalidates the underlying VT session to abort an in-progress synchronous decode, letting the
   * worker thread release device.locker before the main thread enters EAGLDevice::finish(). An
   * in-flight decode is given until deadline to return on its own; only if it is still running past
   * that point is the session invalidated underneath it. The deadline is shared across all decoders
   * so the observer's total wait is capped rather than being the per-decoder budget times the
   * number of decoders. Must be called only by the willResignActive observer while it holds this
   * decoder's lifecycle-token mutex and the token still references the decoder; it is not safe to
   * call from the decode thread itself, which would re-lock decodeLocker.
   */
  void invalidateSession(std::chrono::steady_clock::time_point deadline);

 private:
  bool isInitialized = false;
  VTDecompressionSessionRef session = nullptr;
  std::timed_mutex sessionLocker = {};
  std::timed_mutex decodeLocker = {};
  CMFormatDescriptionRef videoFormatDescription = nullptr;
  tgfx::YUVColorSpace sourceColorSpace = tgfx::YUVColorSpace::BT601_LIMITED;
  tgfx::YUVColorSpace destinationColorSpace = tgfx::YUVColorSpace::BT601_LIMITED;

  explicit HardwareDecoder(const VideoFormat& format);
  bool initVideoToolBox(const std::vector<std::shared_ptr<tgfx::Data>>& headers,
                        const std::string& mimeType);
  bool resetVideoToolBox();
  void cleanResources();

  struct OutputFrame {
    int64_t frameTime = 0;
    CVPixelBufferRef outputPixelBuffer = nullptr;
  };

  int64_t sendFrameTime = -1;
  std::list<int64_t> pendingFrames{};
  std::unordered_map<pag::Frame, OutputFrame*> outputFrameCaches{};
  int maxNumReorder = 0;
  bool inputEndOfStream = false;

  CMBlockBufferRef blockBuffer = nullptr;
  CMSampleBufferRef sampleBuffer = nullptr;
  OutputFrame* outputFrame = nullptr;
  VTDecodeContextPool contextPool = {};
  std::shared_ptr<DecoderToken> lifecycleToken = {};

  friend class HardwareDecoderFactory;
};
}  // namespace pag
