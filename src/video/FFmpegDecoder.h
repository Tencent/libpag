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

#ifdef FFMPEG

#pragma once

#include "VideoDecoder.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"

#ifdef FILE_MOVIE
#include <libswscale/swscale.h>
#endif
}

namespace pag {
class FFmpegFrame {
 public:
  static std::shared_ptr<FFmpegFrame> Make();

  AVFrame* frame() {
    return frame_;
  }

  ~FFmpegFrame();

 private:
  AVFrame* frame_ = nullptr;

  explicit FFmpegFrame(AVFrame* frame) : frame_(frame) {
  }
};

class FFmpegDecoder : public VideoDecoder {
 public:
  ~FFmpegDecoder() override;

  bool onConfigure(const VideoConfig& config);

  DecodingResult onSendBytes(void* bytes, size_t length, int64_t time) override;

  DecodingResult onDecodeFrame() override;

  DecodingResult onEndOfStream() override;

  void onFlush() override;

  int64_t presentationTime() override;

  std::shared_ptr<VideoBuffer> onRenderFrame() override;

 private:
  VideoConfig videoConfig = {};
  const AVCodec* codec = nullptr;
  AVCodecContext* context = nullptr;
  std::shared_ptr<FFmpegFrame> ffmpegFrame = nullptr;
  AVPacket* packet = nullptr;

  // hardware decode
  AVBufferRef* hwDeviceContext = nullptr;
  std::shared_ptr<FFmpegFrame> swFFmpegFrame = nullptr;

  bool openDecoder();

  void closeDecoder();

  bool initFFmpeg(const std::string& mine);

  int calculateExtraDataLength();

  void headersToExtraData(uint8_t* extraData);
};

}  // namespace pag

#endif
