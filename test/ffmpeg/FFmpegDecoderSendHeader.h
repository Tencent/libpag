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

// 外部注入软解，sendHeader方式，仅用于视频序列帧
#include "pag/decoder.h"

extern "C" {
#include "libavcodec/avcodec.h"
}
using namespace pag;
namespace pagTestSendHeader {

class FFmpegDecoderFactory : public SoftwareDecoderFactory {
 public:
  /**
   * Create a software decoder
   */
  std::unique_ptr<SoftwareDecoder> createSoftwareDecoder() override;
};

class FFmpegDecoder : public SoftwareDecoder {
 public:
  ~FFmpegDecoder() override;

  bool onConfigure(const std::vector<HeaderData>& headers, std::string mime, int width,
                   int height) override;

  DecoderResult onSendBytes(void* bytes, size_t length, int64_t frame) override;

  DecoderResult onDecodeFrame() override;

  DecoderResult onEndOfStream() override;

  void onFlush() override;

  std::unique_ptr<YUVBuffer> onRenderFrame() override;

 private:
  const AVCodec* codec = nullptr;
  AVCodecContext* context = nullptr;
  AVFrame* frame = nullptr;
  AVPacket* packet = nullptr;
  std::vector<HeaderData> headers;

  bool openDecoder();
  void closeDecoder();
  bool initFFmpeg();
  void sendHeaders();
};

}  // namespace pagTestSendHeader
