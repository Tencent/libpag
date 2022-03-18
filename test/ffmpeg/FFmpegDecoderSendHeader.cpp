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

#include "FFmpegDecoderSendHeader.h"

namespace pagTestSendHeader {

std::unique_ptr<SoftwareDecoder> FFmpegDecoderFactory::createSoftwareDecoder() {
  av_log_set_level(AV_LOG_FATAL);
  return std::unique_ptr<SoftwareDecoder>(new FFmpegDecoder());
}

FFmpegDecoder::~FFmpegDecoder() {
  closeDecoder();

  if (frame != nullptr) {
    av_frame_free(&frame);
  }
  if (packet != nullptr) {
    av_packet_free(&packet);
  }
}

bool FFmpegDecoder::onConfigure(const std::vector<HeaderData>& headerData, std::string, int, int) {
  this->headers = headerData;
  auto isValid = initFFmpeg();
  if (isValid) {
    sendHeaders();
  }
  return isValid;
}

DecoderResult FFmpegDecoder::onSendBytes(void* bytes, size_t length, int64_t) {
  if (context == nullptr) {
    return DecoderResult::Error;
  }
  packet->data = static_cast<uint8_t*>(bytes);
  packet->size = static_cast<int>(length);
  auto result = avcodec_send_packet(context, packet);
  if (result >= 0 || result == AVERROR_EOF) {
    return DecoderResult::Success;
  } else if (result == AVERROR(EAGAIN)) {
    return DecoderResult::TryAgainLater;
  } else {
    return DecoderResult::Error;
  }
}

DecoderResult FFmpegDecoder::onDecodeFrame() {
  auto result = avcodec_receive_frame(context, frame);
  if (result >= 0 && frame->data[0] != nullptr) {
    return DecoderResult::Success;
  } else if (result == AVERROR(EAGAIN)) {
    return DecoderResult::TryAgainLater;
  } else {
    return DecoderResult::Error;
  }
}

DecoderResult FFmpegDecoder::onEndOfStream() {
  return onSendBytes(nullptr, 0, -1);
}

void FFmpegDecoder::onFlush() {
  closeDecoder();
  if (openDecoder()) {
    sendHeaders();
  }
}

std::unique_ptr<YUVBuffer> FFmpegDecoder::onRenderFrame() {
  auto outputFrame = std::make_unique<YUVBuffer>();
  for (int i = 0; i < 3; i++) {
    outputFrame->data[i] = frame->data[i];
    outputFrame->lineSize[i] = frame->linesize[i];
  }
  return outputFrame;
}

void FFmpegDecoder::sendHeaders() {
  for (auto& head : headers) {
    onSendBytes(head.data, head.length, 0);
  }
}

bool FFmpegDecoder::initFFmpeg() {
  packet = av_packet_alloc();
  if (!packet) {
    return false;
  }
  codec = avcodec_find_decoder(AV_CODEC_ID_H264);
  if (!codec) {
    return false;
  }
  if (!openDecoder()) {
    return false;
  }
  frame = av_frame_alloc();
  return frame != nullptr;
}

bool FFmpegDecoder::openDecoder() {
  context = avcodec_alloc_context3(codec);
  if (!context) {
    return false;
  }
  return avcodec_open2(context, codec, nullptr) >= 0;
}

void FFmpegDecoder::closeDecoder() {
  if (context != nullptr) {
    avcodec_free_context(&context);
    context = nullptr;
  }
}

}  // namespace pagTestSendHeader
