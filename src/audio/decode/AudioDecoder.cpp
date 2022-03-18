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

#ifdef FILE_MOVIE

#include "AudioDecoder.h"

namespace pag {
std::unique_ptr<AudioDecoder> AudioDecoder::Make(AVStream* stream,
                                                 std::shared_ptr<PCMOutputConfig> outputConfig) {
  if (stream == nullptr) {
    return nullptr;
  }
  auto decoder = std::unique_ptr<AudioDecoder>(new AudioDecoder());
  if (decoder->open(stream, std::move(outputConfig)) < 0) {
    return nullptr;
  }
  return decoder;
}

AudioDecoder::~AudioDecoder() {
  delete converter;
  avcodec_free_context(&avCodecContext);
  av_frame_free(&frame);
  av_packet_free(&packet);
}

DecodingResult AudioDecoder::onEndOfStream() {
  return onSendBytes(nullptr, 0, -1);
}

DecodingResult AudioDecoder::onSendBytes(void* bytes, size_t length, int64_t time) {
  packet->data = static_cast<uint8_t*>(bytes);
  packet->size = static_cast<int>(length);
  packet->pts = time;
  int result = avcodec_send_packet(avCodecContext, packet);
  if (result >= 0 || result == AVERROR_EOF) {
    return DecodingResult::Success;
  } else if (result == AVERROR(EAGAIN)) {
    return DecodingResult::TryAgainLater;
  } else {
    return DecodingResult::Error;
  }
}

DecodingResult AudioDecoder::onDecodeFrame() {
  auto result = avcodec_receive_frame(avCodecContext, frame);
  if (result == 0 && frame->data[0] != nullptr) {
    return DecodingResult::Success;
  } else if (result == AVERROR(EAGAIN)) {
    return DecodingResult::TryAgainLater;
  } else if (result == AVERROR_EOF) {
    return DecodingResult::EndOfStream;
  } else {
    return DecodingResult::Error;
  }
}

void AudioDecoder::onFlush() const {
  avcodec_flush_buffers(avCodecContext);
}

SampleData AudioDecoder::onRenderFrame() {
  SampleData data;
  if (*outputConfig == *frame) {
    data = SampleData(frame->data[0], frame->linesize[0]);
  } else {
    if (converter == nullptr) {
      converter = new AudioFormatConverter(outputConfig);
    }
    data = converter->convert(frame);
  }
  return data;
}

int64_t AudioDecoder::currentPresentationTime() {
  if (frame == nullptr) {
    return -1;
  }
  return frame->pts;
}

int AudioDecoder::open(AVStream* stream, std::shared_ptr<PCMOutputConfig> config) {
  auto dec = avcodec_find_decoder(stream->codecpar->codec_id);
  if (!dec) {
    LOGE("Failed to find codec\n");
    return -1;
  }
  avCodecContext = avcodec_alloc_context3(dec);
  if (!avCodecContext) {
    LOGE("Failed to allocate codec context\n");
    return -1;
  }
  if (avcodec_parameters_to_context(avCodecContext, stream->codecpar) < 0) {
    LOGE("Failed to copy  codec parameters to decoder context\n");
    return -1;
  }
  AVDictionary* opts = nullptr;
  av_dict_set(&opts, "refcounted_frames", "0", 0);
  if (avcodec_open2(avCodecContext, dec, &opts) < 0) {
    LOGE("Failed to open codec\n");
    return -1;
  }
  avCodecContext->pkt_timebase = stream->time_base;
  frame = av_frame_alloc();
  if (!frame) {
    LOGE("Could not allocate frame\n");
    return AVERROR(ENOMEM);
  }
  packet = av_packet_alloc();
  if (!packet) {
    return -1;
  }
  outputConfig = std::move(config);
  return 0;
}
}  // namespace pag
#endif
