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

#include "DecodingPolicy.h"
#include "MediaDemuxer.h"
#include "VideoDecoder.h"
#include "base/utils/Task.h"
#include "rendering/Performance.h"

namespace pag {
class VideoReader {
 public:
  VideoReader(VideoConfig videoConfig, std::unique_ptr<MediaDemuxer> demuxer,
              DecodingPolicy policy = DecodingPolicy::Hardware);

  ~VideoReader();

  int64_t getSampleTimeAt(int64_t targetTime);

  /**
   * 如果不存在有效的下一帧，返回 INT64_MAX。
   */
  int64_t getNextSampleTimeAt(int64_t targetTime);

  std::shared_ptr<VideoBuffer> readSample(int64_t targetTime);

  void reportPerformance(Performance* performance, int64_t decodingTime);

 private:
  std::mutex locker = {};
  VideoConfig videoConfig = {};
  MediaDemuxer* demuxer = nullptr;
  std::shared_ptr<Task> gpuDecoderTask = nullptr;
  VideoDecoder* videoDecoder = nullptr;
  int decoderTypeIndex = 0;

  std::shared_ptr<VideoBuffer> outputBuffer = nullptr;
  bool outputEndOfStream = false;
  bool needsAdvance = false;
  bool inputEndOfStream = false;
  int64_t currentDecodedTime = INT64_MIN;
  int64_t currentRenderedTime = INT64_MIN;

  int64_t hardDecodingInitialTime = 0;
  int64_t softDecodingInitialTime = 0;

  void destroyVideoDecoder();

  void tryMakeVideoDecoder();

  void resetParams();

  bool sendData();

  bool decodeFrame(int64_t sampleTime);

  bool onDecodeFrame(int64_t sampleTime);

  bool switchToGPUDecoderOfTask();

  bool renderFrame(int64_t sampleTime);

  VideoDecoder* makeDecoder();
};
}  // namespace pag
