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

// 外部注入软解，构建AVCodecParameters模式，接收Header数据AnnexB，解码数据AVCC,验证视频序列帧和视频

#include "pag/decoder.h"

extern "C" {
#include "libavcodec/avcodec.h"
}

namespace pagTest {

class FFmpegDecoderFactory : public pag::SoftwareDecoderFactory {
 public:
  /**
   * Create a software decoder
   */
  std::unique_ptr<pag::SoftwareDecoder> createSoftwareDecoder() override;
};

class FFmpegDecoder : public pag::SoftwareDecoder {
 public:
  ~FFmpegDecoder() override;

  bool onConfigure(const std::vector<pag::HeaderData>& headers, std::string mime, int width,
                   int height) override;

  pag::DecoderResult onSendBytes(void* bytes, size_t length, int64_t frame) override;

  pag::DecoderResult onDecodeFrame() override;

  pag::DecoderResult onEndOfStream() override;

  void onFlush() override;

  std::unique_ptr<pag::YUVBuffer> onRenderFrame() override;

 private:
  const AVCodec* codec = nullptr;
  AVCodecContext* context = nullptr;
  AVFrame* frame = nullptr;
  AVPacket* packet = nullptr;
  std::vector<pag::HeaderData> headers;
  std::string mime;

  bool openDecoder();
  void closeDecoder();
  bool initFFmpeg(const std::string& mine);
  int calculateExtraDataLength();
  void headersToExtraData(uint8_t* extraData);
};

}  // namespace pagTest
