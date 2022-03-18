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

#include "FFmpegDecoder.h"

namespace pagTest {

AVCodecID MineStringToAVCodecID(const std::string& mime) {
  AVCodecID codecId = AVCodecID::AV_CODEC_ID_H264;
  if (mime.empty()) {
    return codecId;
  }
  if (mime == "video/avc") {
    codecId = AVCodecID::AV_CODEC_ID_H264;
  } else if (mime == "video/hevc") {
    codecId = AVCodecID::AV_CODEC_ID_HEVC;
  }
  return codecId;
}

std::unique_ptr<pag::SoftwareDecoder> FFmpegDecoderFactory::createSoftwareDecoder() {
  av_log_set_level(AV_LOG_FATAL);
  return std::unique_ptr<pag::SoftwareDecoder>(new FFmpegDecoder());
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

bool FFmpegDecoder::onConfigure(const std::vector<pag::HeaderData>& headerData,
                                std::string mimeType, int, int) {
  this->headers = headerData;
  this->mime = mimeType;
  auto isValid = initFFmpeg(mime);
  return isValid;
}

pag::DecoderResult FFmpegDecoder::onSendBytes(void* bytes, size_t length, int64_t) {
  if (context == nullptr) {
    return pag::DecoderResult::Error;
  }
  packet->data = static_cast<uint8_t*>(bytes);
  packet->size = static_cast<int>(length);
  auto result = avcodec_send_packet(context, packet);
  if (result >= 0 || result == AVERROR_EOF) {
    return pag::DecoderResult::Success;
  } else if (result == AVERROR(EAGAIN)) {
    return pag::DecoderResult::TryAgainLater;
  } else {
    return pag::DecoderResult::Error;
  }
}

pag::DecoderResult FFmpegDecoder::onDecodeFrame() {
  auto result = avcodec_receive_frame(context, frame);
  if (result >= 0 && frame->data[0] != nullptr) {
    return pag::DecoderResult::Success;
  } else if (result == AVERROR(EAGAIN)) {
    return pag::DecoderResult::TryAgainLater;
  } else {
    return pag::DecoderResult::Error;
  }
}

pag::DecoderResult FFmpegDecoder::onEndOfStream() {
  return onSendBytes(nullptr, 0, -1);
}

void FFmpegDecoder::onFlush() {
  avcodec_flush_buffers(context);
}

std::unique_ptr<pag::YUVBuffer> FFmpegDecoder::onRenderFrame() {
  auto outputFrame = std::make_unique<pag::YUVBuffer>();
  for (int i = 0; i < 3; i++) {
    outputFrame->data[i] = frame->data[i];
    outputFrame->lineSize[i] = frame->linesize[i];
  }
  return outputFrame;
}

bool FFmpegDecoder::initFFmpeg(const std::string& mimeType) {
  packet = av_packet_alloc();
  if (!packet) {
    return false;
  }

  codec = avcodec_find_decoder(MineStringToAVCodecID(mimeType));
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
  AVCodecParameters codecpar = {};
  int extraDataSize = calculateExtraDataLength();
  auto extraData = new uint8_t[extraDataSize];
  headersToExtraData(extraData);

  codecpar.extradata = extraData;
  codecpar.extradata_size = extraDataSize;

  if ((avcodec_parameters_to_context(context, &codecpar)) < 0) {
    printf("Failed to copy  codec parameters to decoder context\n");
  }
  bool result = avcodec_open2(context, codec, nullptr) >= 0;
  delete[] extraData;
  return result;
}

void FFmpegDecoder::closeDecoder() {
  if (context != nullptr) {
    avcodec_free_context(&context);
    context = nullptr;
  }
}

int DataSplice(uint8_t* target, pag::HeaderData header, int pos) {
  uint16_t headerLength = header.length - 4;
  target[pos++] = (headerLength >> 8u);
  target[pos++] = (headerLength & 0xFFu);
  memcpy(target + pos, header.data + 4, header.length - 4);
  pos += header.length - 4;
  return pos;
}

int FFmpegDecoder::calculateExtraDataLength() {
  int extraDataSize = 0;
  for (auto& header : headers) {
    extraDataSize += header.length;
  }
  return extraDataSize;
}

void FFmpegDecoder::headersToExtraData(uint8_t* extraData) {
  int pos = 0;
  for (auto& header : headers) {
    memcpy(extraData + pos, header.data, header.length);
    pos += header.length;
  }
}

}  // namespace pagTest
