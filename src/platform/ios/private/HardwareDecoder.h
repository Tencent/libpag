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

#include <CoreMedia/CoreMedia.h>
#include <VideoToolbox/VideoToolbox.h>
#include <list>
#include <unordered_map>
#include "rendering/video/VideoDecoder.h"

namespace pag {
class HardwareDecoder : public VideoDecoder {
 public:
  ~HardwareDecoder() override;

  pag::DecodingResult onSendBytes(void* bytes, size_t length, int64_t time) override;

  pag::DecodingResult onEndOfStream() override;

  pag::DecodingResult onDecodeFrame() override;

  void onFlush() override;

  int64_t presentationTime() override;

  std::shared_ptr<tgfx::ImageBuffer> onRenderFrame() override;

 private:
  bool isInitialized = false;
  VTDecompressionSessionRef session = nullptr;
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

  friend class HardwareDecoderFactory;
};
}  // namespace pag
