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

#include "FFmpegDecoder.h"
#include <algorithm>
#include "I420Buffer.h"
#include "NV12Buffer.h"
#include "platform/Platform.h"

namespace pag {
static enum AVPixelFormat hardwarePixelFormat = AV_PIX_FMT_NONE;

static AVHWDeviceType FindHardwarDeviceType(const AVCodec* codec) {
  std::string deviceType;
  AVHWDeviceType hwDeviceType = AV_HWDEVICE_TYPE_NONE;
  while ((hwDeviceType = av_hwdevice_iterate_types(hwDeviceType)) != AV_HWDEVICE_TYPE_NONE) {
    deviceType = av_hwdevice_get_type_name(hwDeviceType);
  }
  if (deviceType.empty()) {
    return AV_HWDEVICE_TYPE_NONE;
  }
  hwDeviceType = av_hwdevice_find_type_by_name(deviceType.c_str());
  if (hwDeviceType == AV_HWDEVICE_TYPE_NONE) {
    return AV_HWDEVICE_TYPE_NONE;
  }

  for (int i = 0;; i++) {
    const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
    if (!config) {
      return AV_HWDEVICE_TYPE_NONE;
    } else if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
               config->device_type == hwDeviceType) {
      hardwarePixelFormat = config->pix_fmt;
      break;
    }
  }
  return hwDeviceType;
}

std::shared_ptr<FFmpegFrame> FFmpegFrame::Make() {
  auto frame = av_frame_alloc();
  if (frame == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<FFmpegFrame>(new FFmpegFrame(frame));
}

FFmpegFrame::~FFmpegFrame() {
  if (frame_ != nullptr) {
    av_frame_free(&frame_);
  }
}

class FFmpegI420Buffer : public I420Buffer {
 public:
  static std::shared_ptr<FFmpegI420Buffer> Make(int width, int height,
                                                tgfx::YUVColorSpace colorSpace, tgfx::YUVColorRange colorRange,
                                                std::shared_ptr<FFmpegFrame> frame) {
    auto buffer = new FFmpegI420Buffer(width, height, colorSpace, colorRange, std::move(frame));
    return std::shared_ptr<FFmpegI420Buffer>(buffer);
  }

 private:
  // 持有 frame 防止 data 被提前释放
  std::shared_ptr<FFmpegFrame> frame = nullptr;

  FFmpegI420Buffer(int width, int height, tgfx::YUVColorSpace colorSpace, tgfx::YUVColorRange colorRange,
                   std::shared_ptr<FFmpegFrame> frame)
      : I420Buffer(width, height, frame->frame()->data, frame->frame()->linesize,
                   colorSpace, colorRange),
        frame(std::move(frame)) {
  }
};

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

bool FFmpegDecoder::onConfigure(const VideoConfig& config) {
  videoConfig = config;
  auto isValid = initFFmpeg(config.mimeType);
  return isValid;
}

FFmpegDecoder::~FFmpegDecoder() {
  closeDecoder();
  if (packet != nullptr) {
    av_packet_free(&packet);
  }
  av_buffer_unref(&hwDeviceContext);
}

DecodingResult FFmpegDecoder::onSendBytes(void* bytes, size_t length, int64_t time) {
  if (context == nullptr) {
    return DecodingResult::Error;
  }
  packet->data = static_cast<uint8_t*>(bytes);
  packet->size = static_cast<int>(length);
  packet->pts = time;

  auto result = avcodec_send_packet(context, packet);

  if (result >= 0 || result == AVERROR_EOF) {
    return DecodingResult::Success;
  } else if (result == AVERROR(EAGAIN)) {
    return DecodingResult::TryAgainLater;
  } else {
    LOGE("FFmpegDecoder: Error on sending bytes for decoding, time:%lld \n", time);
    return DecodingResult::Error;
  }
}

DecodingResult FFmpegDecoder::onDecodeFrame() {
  auto result = avcodec_receive_frame(context, ffmpegFrame->frame());
  if (result == 0) {
    return DecodingResult::Success;
  } else if (result == AVERROR(EAGAIN)) {
    return DecodingResult::TryAgainLater;
  } else if (result == AVERROR_EOF) {
    return DecodingResult::EndOfStream;
  } else {
    LOGE("FFmpegDecoder: Error on decoding frame.\n");
    return DecodingResult::Error;
  }
}

DecodingResult FFmpegDecoder::onEndOfStream() {
  return onSendBytes(nullptr, 0, -1);
}

void FFmpegDecoder::onFlush() {
  avcodec_flush_buffers(context);
}

int64_t FFmpegDecoder::presentationTime() {
  if (ffmpegFrame == nullptr) {
    return -1;
  }
  return ffmpegFrame->frame()->pts;
}

std::shared_ptr<VideoBuffer> GetVideoBuffer(const VideoConfig& config,
                                            const std::shared_ptr<FFmpegFrame>& frame) {
  switch (frame->frame()->format) {
    case AV_PIX_FMT_YUV420P:
    case AV_PIX_FMT_YUVJ420P:
      return FFmpegI420Buffer::Make(config.width, config.height,
                                    config.colorSpace, config.colorRange, frame);
    case AV_PIX_FMT_NV12:
      return NV12Buffer::Make(config.width, config.height,
                              config.colorSpace, config.colorRange, frame);
    default:
      break;
  }
  LOGE("The format decoded by FFmpeg is not supported， format:%d !\n", frame->frame()->format);
  return nullptr;
}

std::shared_ptr<VideoBuffer> FFmpegDecoder::onRenderFrame() {
  if (ffmpegFrame->frame()->format != AV_PIX_FMT_NONE &&
      ffmpegFrame->frame()->format == hardwarePixelFormat) {
    if (swFFmpegFrame == nullptr) {
      swFFmpegFrame = FFmpegFrame::Make();
    }
    if (av_hwframe_transfer_data(swFFmpegFrame->frame(), ffmpegFrame->frame(), 0) < 0) {
      LOGE("Error transferring the data from GPU to system memory, original format:%d !\n",
           ffmpegFrame->frame()->format);
      return nullptr;
    }
    return GetVideoBuffer(videoConfig, swFFmpegFrame);
  }
  return GetVideoBuffer(videoConfig, ffmpegFrame);
}

bool FFmpegDecoder::initFFmpeg(const std::string& mine) {
  packet = av_packet_alloc();
  if (!packet) {
    return false;
  }
  codec = avcodec_find_decoder(MineStringToAVCodecID(mine));
  if (!codec) {
    return false;
  }
  if (!openDecoder()) {
    return false;
  }
  ffmpegFrame = FFmpegFrame::Make();
  av_log_set_level(AV_LOG_FATAL);
  return ffmpegFrame != nullptr;
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
  // FFmpeg h264解码需要设置video_delay，未设置含有B帧视频解码会出现异常，第一个B帧无法解码成功，
  // 返回TryAgainLater
  if (codec->id == AVCodecID::AV_CODEC_ID_H264) {
    codecpar.video_delay = 1;
  }

  if ((avcodec_parameters_to_context(context, &codecpar)) < 0) {
    LOGE("Failed to copy  codec parameters to decoder context. \n");
  }

  static AVHWDeviceType hwDeviceType = FindHardwarDeviceType(codec);
  if (hwDeviceType != AV_HWDEVICE_TYPE_NONE) {
    if (av_hwdevice_ctx_create(&hwDeviceContext, hwDeviceType, nullptr, nullptr, 0) == 0) {
      context->hw_device_ctx = av_buffer_ref(hwDeviceContext);
    } else {
      LOGI("Failed to create specified ffmpeg hardware device, use software decoder. \n");
    }
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

static int DataSplice(uint8_t* target, ByteData* header, int pos) {
  uint16_t headerLength = header->length() - 4;
  target[pos++] = (headerLength >> 8u);
  target[pos++] = (headerLength & 0xFFu);
  memcpy(target + pos, header->data() + 4, header->length() - 4);
  pos += static_cast<int>(header->length()) - 4;
  return pos;
}

int FFmpegDecoder::calculateExtraDataLength() {
  size_t extraDataSize = 0;
  auto& headers = videoConfig.headers;
  if (Platform::Current()->naluType() != NALUType::AnnexB) {
    int sliceDeviation;
    if (videoConfig.mimeType == "video/hevc") {
      extraDataSize = 23;
      sliceDeviation = 1;
    } else {
      extraDataSize = 7;
      sliceDeviation = -2;
    }
    for (size_t i = 0; i < headers.size(); i++) {
      if (i == 3) {
        extraDataSize += headers[i]->length() - 2;
      } else {
        extraDataSize += headers[i]->length() + sliceDeviation;
      }
    }
  } else {
    for (auto& header : headers) {
      extraDataSize += header->length();
    }
  }
  return static_cast<int>(extraDataSize);
}

void FFmpegDecoder::headersToExtraData(uint8_t* extraData) {
  int pos = 0;
  auto& headers = videoConfig.headers;
  if (Platform::Current()->naluType() != NALUType::AnnexB) {
    extraData[pos++] = 1;
    if (videoConfig.mimeType == "video/hevc") {
      memset(extraData + 1, 0, 20);
      pos += 20;
      extraData[pos++] = 0x0b;
      extraData[pos++] = 3;
      for (size_t i = 0; i < headers.size(); i++) {
        if (i < 3) {
          extraData[pos++] = static_cast<uint8_t>(i) | 0xa0u;
          extraData[pos++] = 0;
          if (i == 2 && headers.size() == 4) {
            extraData[pos++] = 2;
          } else {
            extraData[pos++] = 1;
          }
        }
        pos = DataSplice(extraData, headers[i].get(), pos);
      }
    } else {
      auto firstData = headers[0]->data();
      extraData[pos++] = firstData[5];
      extraData[pos++] = firstData[6];
      extraData[pos++] = firstData[7];
      extraData[pos++] = 0xff;
      extraData[pos++] = 0xe1;
      for (size_t i = 0; i < headers.size(); i++) {
        if (i > 0) {
          extraData[pos++] = 1;
        }
        pos = DataSplice(extraData, headers[i].get(), pos);
      }
    }
  } else {
    for (auto& header : headers) {
      memcpy(extraData + pos, header->data(), header->length());
      pos += static_cast<int>(header->length());
    }
  }
}
}  // namespace pag

#endif
