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

#include "HardwareDecoder.h"
#include "base/utils/Log.h"
#include "tgfx/core/Buffer.h"
#include "tgfx/core/Task.h"

namespace pag {

static const int TIMEOUT_US = 1000;

HardwareDecoder::HardwareDecoder(const VideoFormat& format) {
  isValid = initDecoder(format);
}

HardwareDecoder::~HardwareDecoder() {
  releaseOutputBuffer(false);
  if (videoDecoder != nullptr) {
    media_status_t status;
    status = AMediaCodec_stop(videoDecoder);
    if (status != AMEDIA_OK) {
      LOGE("HardwareDecoder: Error on stopping videoDecoder.\n");
    }

    status = AMediaCodec_delete(videoDecoder);
    if (status != AMEDIA_OK) {
      LOGE("HardwareDecoder: Error on deleting videoDecoder.\n");
    }
    videoDecoder = nullptr;
  }
  if (nativeWindow != nullptr) {
    ANativeWindow_release(nativeWindow);
    nativeWindow = nullptr;
  }

  if (bufferInfo != nullptr) {
    delete bufferInfo;
    bufferInfo = nullptr;
  }
}

bool HardwareDecoder::initDecoder(const VideoFormat& format) {
  LOGE("HardwareDecoder: START Hardware Decoder.\n");

  auto mediaFormat = AMediaFormat_new();
  AMediaFormat_setString(mediaFormat, AMEDIAFORMAT_KEY_MIME, format.mimeType.c_str());
  AMediaFormat_setInt32(mediaFormat, AMEDIAFORMAT_KEY_WIDTH, format.width);
  AMediaFormat_setInt32(mediaFormat, AMEDIAFORMAT_KEY_HEIGHT, format.height);

  if (format.mimeType == "video/hevc") {
    if (!format.headers.empty()) {
      const char* keyString = "csd-0";
      size_t dataLength = 0;
      for (auto& header : format.headers) {
        dataLength += header->size();
      }
      tgfx::Buffer buffer(dataLength);
      size_t pos = 0;
      for (auto& header : format.headers) {
        buffer.writeRange(pos, header->size(), header->data());
        pos += header->size();
      }
      AMediaFormat_setBuffer(mediaFormat, keyString, buffer.data(), buffer.size());
    }
  } else {
    int index = 0;
    for (auto& header : format.headers) {
      char keyString[6];
      snprintf(keyString, 6, "csd-%d", index);
      AMediaFormat_setBuffer(mediaFormat, keyString, const_cast<uint8_t*>(header->bytes()),
                             header->size());
      index++;
    }
  }

  AMediaFormat_setFloat(mediaFormat, AMEDIAFORMAT_KEY_FRAME_RATE, format.frameRate);
  videoDecoder = AMediaCodec_createDecoderByType(format.mimeType.c_str());
  if (videoDecoder == nullptr) {
    AMediaFormat_delete(mediaFormat);
    return false;
  }

  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    AMediaFormat_delete(mediaFormat);
    AMediaCodec_delete(videoDecoder);
    videoDecoder = nullptr;
    return false;
  }

  auto videoSurface = JVideoSurface::Make(env, format.width, format.height);
  if (videoSurface == nullptr) {
    AMediaFormat_delete(mediaFormat);
    AMediaCodec_delete(videoDecoder);
    videoDecoder = nullptr;
    return false;
  }

  imageReader = JVideoSurface::GetImageReader(env, videoSurface);
  if (imageReader == nullptr) {
    AMediaFormat_delete(mediaFormat);
    AMediaCodec_delete(videoDecoder);
    videoDecoder = nullptr;
    return false;
  }

  auto surface = imageReader->getInputSurface();
  if (surface == nullptr) {
    AMediaFormat_delete(mediaFormat);
    AMediaCodec_delete(videoDecoder);
    videoDecoder = nullptr;
    return false;
  }

  nativeWindow = ANativeWindow_fromSurface(env, surface);

  media_status_t status;
  status = AMediaCodec_configure(videoDecoder, mediaFormat, nativeWindow, nullptr, 0);
  AMediaFormat_delete(mediaFormat);
  if (status != AMEDIA_OK) {
    LOGE("HardwareDecoder: Error on configuring videoDecoder.\n");
    ANativeWindow_release(nativeWindow);
    AMediaCodec_delete(videoDecoder);
    videoDecoder = nullptr;
    return false;
  }

  status = AMediaCodec_start(videoDecoder);
  if (status != AMEDIA_OK) {
    LOGE("HardwareDecoder: Error on starting videoDecoder.\n");
    ANativeWindow_release(nativeWindow);
    AMediaCodec_delete(videoDecoder);
    videoDecoder = nullptr;
    return false;
  }

  bufferInfo = new AMediaCodecBufferInfo();
  return true;
}

DecodingResult HardwareDecoder::onSendBytes(void* bytes, size_t length, int64_t time) {
  ssize_t inputBufferIndex = AMediaCodec_dequeueInputBuffer(videoDecoder, TIMEOUT_US);
  size_t outsize;
  if (inputBufferIndex >= 0) {
    uint8_t* inputBuffer = AMediaCodec_getInputBuffer(videoDecoder, inputBufferIndex, &outsize);
    if (inputBuffer == nullptr || length > outsize) {
      LOGE("HardwareDecoder: Error on getting input buffer index.\n");
      return pag::DecodingResult::Error;
    }
    memcpy(inputBuffer, bytes, length);
    if (AMediaCodec_queueInputBuffer(videoDecoder, inputBufferIndex, 0, length, time, 0) !=
        AMEDIA_OK) {
      LOGE("HardwareDecoder: Error on queuing input buffer in onSendBytes.\n");
      return pag::DecodingResult::Error;
    }
    disableFlush = false;
    return pag::DecodingResult::Success;
  }
  return pag::DecodingResult::TryAgainLater;
}

DecodingResult HardwareDecoder::onDecodeFrame() {
  releaseOutputBuffer(false);
  ssize_t outputBufferIndex = AMediaCodec_dequeueOutputBuffer(videoDecoder, bufferInfo, TIMEOUT_US);
  if ((bufferInfo->flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0) {
    if (outputBufferIndex >= 0) {
      lastOutputBufferIndex = outputBufferIndex;
    }
    return pag::DecodingResult::EndOfStream;
  }
  if (outputBufferIndex >= 0) {
    lastOutputBufferIndex = outputBufferIndex;
  }

  return lastOutputBufferIndex != -1 ? pag::DecodingResult::Success
                                     : pag::DecodingResult::TryAgainLater;
}

void HardwareDecoder::onFlush() {
  if (disableFlush) {
    return;
  }
  if (AMediaCodec_flush(videoDecoder) != AMEDIA_OK) {
    LOGE("HardwareDecoder: Error on Decoder Flush.");
  }

  bufferInfo->flags = 0;
  bufferInfo->presentationTimeUs = 0;

  lastOutputBufferIndex = -1;
}

int64_t HardwareDecoder::presentationTime() {
  return bufferInfo->presentationTimeUs;
}

DecodingResult HardwareDecoder::onEndOfStream() {
  ssize_t inputBufferIndex = AMediaCodec_dequeueInputBuffer(videoDecoder, TIMEOUT_US);
  if (inputBufferIndex >= 0) {
    if (AMediaCodec_queueInputBuffer(videoDecoder, inputBufferIndex, 0, 0, 0,
                                     AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != AMEDIA_OK) {
      LOGE("HardwareDecoder: Error on queuing input buffer in EndOfStream.\n");
      return pag::DecodingResult::Error;
    }
    disableFlush = false;
    return pag::DecodingResult::Success;
  }
  return pag::DecodingResult::TryAgainLater;
}

std::shared_ptr<tgfx::ImageBuffer> HardwareDecoder::onRenderFrame() {
  bool result = releaseOutputBuffer(true);
  if (!result) {
    return nullptr;
  }
  return imageReader->acquireNextBuffer();
}

bool HardwareDecoder::releaseOutputBuffer(bool render) {
  if (lastOutputBufferIndex != -1) {
    if (AMediaCodec_releaseOutputBuffer(videoDecoder, lastOutputBufferIndex, render) == AMEDIA_OK) {
      lastOutputBufferIndex = -1;
      return true;
    }
    lastOutputBufferIndex = -1;
    LOGE("HardwareDecoder: Error on releasing Output Buffer.");
  }
  return false;
}

}  // namespace pag
