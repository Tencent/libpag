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
#ifdef FILE_MOVIE

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#ifdef __cplusplus
}
#endif

#include <memory>
#include "audio/process/AudioFormatConverter.h"
#include "audio/utils/AudioUtils.h"
#include "video/FFmpegDemuxer.h"
#include "video/VideoDecoder.h"

namespace pag {
class AudioDecoder {
 public:
  static std::unique_ptr<AudioDecoder> Make(AVStream* stream,
                                            std::shared_ptr<PCMOutputConfig> outputConfig);

  ~AudioDecoder();

  AVCodecContext* avCodecContext = nullptr;

  DecodingResult onEndOfStream();

  DecodingResult onSendBytes(void* bytes, size_t length, int64_t time);

  DecodingResult onDecodeFrame();

  void onFlush() const;

  SampleData onRenderFrame();

  int64_t currentPresentationTime();

 private:
  std::shared_ptr<PCMOutputConfig> outputConfig = nullptr;
  AudioFormatConverter* converter = nullptr;
  AVPacket* packet = nullptr;
  AVFrame* frame = nullptr;

  AudioDecoder() = default;

  int open(AVStream* stream, std::shared_ptr<PCMOutputConfig> config);
};
}  // namespace pag

#endif
