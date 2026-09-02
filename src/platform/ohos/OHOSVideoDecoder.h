/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <multimedia/player_framework/native_avcapability.h>
#include <multimedia/player_framework/native_avcodec_videodecoder.h>
#include <atomic>
#include <cstdint>
#include <list>
#include <queue>
#include "pag/decoder.h"
#include "rendering/video/DecodingResult.h"
#include "rendering/video/VideoDecoder.h"

namespace pag {
struct CodecBufferInfo {
  uint32_t bufferIndex = 0;
  OH_AVBuffer* buffer = nullptr;
  OH_AVCodecBufferAttr attr = {0, 0, 0, AVCODEC_BUFFER_FLAGS_NONE};

  CodecBufferInfo(uint32_t argBufferIndex, OH_AVBuffer* argBuffer)
      : bufferIndex(argBufferIndex), buffer(argBuffer) {
    // Guard against a null buffer (e.g. the (0, nullptr) sentinel pushed by the error callback):
    // OH_AVBuffer_GetBufferAttr is not documented to tolerate a null buffer on every HarmonyOS
    // build, and attr already holds a valid zero-initialized default here.
    if (argBuffer != nullptr) {
      OH_AVBuffer_GetBufferAttr(argBuffer, &attr);
    }
  };
};

class CodecUserData {
 public:
  std::mutex inputMutex;
  std::condition_variable inputCondition;
  std::queue<CodecBufferInfo> inputBufferInfoQueue;

  std::mutex outputMutex;
  std::condition_variable outputCondition;
  std::queue<CodecBufferInfo> outputBufferInfoQueue;

  // Set when the codec enters an error state it cannot recover from on its own, either by the error
  // callback or by onSendBytes after it dequeues an unusable input buffer (null/undersized) or fails
  // to submit one. The input waiter checks it directly so it can bail out of onSendBytes instead of
  // blocking forever on a codec that will never redeliver the dequeued input buffers; the output
  // waiter is not gated on this flag but is instead woken by a sentinel (0, nullptr) entry the error
  // callback pushes into outputBufferInfoQueue. onFlush clears it to allow recovery.
  std::atomic<bool> codecError = {false};

  void clearQueue() {
    {
      std::unique_lock<std::mutex> lock(inputMutex);
      auto emptyQueue = std::queue<CodecBufferInfo>();
      inputBufferInfoQueue.swap(emptyQueue);
    }
    {
      std::unique_lock<std::mutex> lock(outputMutex);
      auto emptyQueue = std::queue<CodecBufferInfo>();
      outputBufferInfoQueue.swap(emptyQueue);
    }
  }
};

class OHOSVideoDecoder : public VideoDecoder {
 public:
  static std::unique_ptr<OHOSVideoDecoder> MakeHardwareDecoder(const VideoFormat& format);

  static std::shared_ptr<OHOSVideoDecoder> MakeSoftwareDecoder(const VideoFormat& format);

  ~OHOSVideoDecoder() override;

  DecodingResult onSendBytes(void* bytes, size_t length, int64_t time) override;

  DecodingResult onEndOfStream() override;

  DecodingResult onDecodeFrame() override;

  void onFlush() override;

  int64_t presentationTime() override;

  std::shared_ptr<tgfx::ImageBuffer> onRenderFrame() override;

 private:
  bool isValid = false;
  OH_AVCodec* videoCodec = nullptr;
  CodecUserData* codecUserData = nullptr;
  CodecBufferInfo codecBufferInfo = {0, nullptr};
  VideoFormat videoFormat{};
  std::list<int64_t> pendingFrames{};
  OH_AVCodecCategory codecCategory = HARDWARE;
  int videoStride = 0;
  int videoSliceHeight = 0;
  size_t maxPendingFramesCount = 0;

  explicit OHOSVideoDecoder(const VideoFormat& format, bool hardware);
  bool initDecoder(const OH_AVCodecCategory avCodecCategory);
  bool start();
  void releaseOutputBuffer();
};
}  // namespace pag
