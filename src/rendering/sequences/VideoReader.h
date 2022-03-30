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

#include "SequenceReader.h"
#include "rendering/video/VideoDecoder.h"
#include "rendering/video/VideoDemuxer.h"

namespace pag {
class VideoReader : public SequenceReader {
 public:
  explicit VideoReader(std::unique_ptr<VideoDemuxer> demuxer);

  ~VideoReader() override;

 protected:
  bool decodeFrame(Frame targetFrame) override;

  std::shared_ptr<tgfx::Texture> makeTexture(tgfx::Context* context) override;

  void recordPerformance(Performance* performance, int64_t decodingTime) override;

 private:
  std::mutex locker = {};
  VideoDemuxer* demuxer = nullptr;
  float frameRate = 0.0;
  int decoderTypeIndex = 0;
  std::shared_ptr<Task> gpuDecoderTask = nullptr;
  VideoDecoder* videoDecoder = nullptr;
  VideoSample videoSample = {};
  std::shared_ptr<VideoBuffer> lastBuffer = nullptr;
  bool outputEndOfStream = false;
  bool inputEndOfStream = false;
  int64_t currentDecodedTime = INT64_MIN;
  int64_t currentRenderedTime = INT64_MIN;
  int64_t hardDecodingInitialTime = 0;
  int64_t softDecodingInitialTime = 0;

  void destroyVideoDecoder();

  bool checkVideoDecoder();

  void resetParams();

  bool sendSampleData();

  bool onDecodeFrame(int64_t sampleTime);

  bool switchToGPUDecoderOfTask();

  VideoDecoder* makeVideoDecoder();
};
}  // namespace pag
