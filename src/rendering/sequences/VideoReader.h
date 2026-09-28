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

#pragma once

#include <atomic>
#include <vector>
#include "SequenceReader.h"
#include "rendering/video/VideoDecoderFactory.h"
#include "rendering/video/VideoDemuxer.h"

namespace pag {
class VideoReader : public SequenceReader {
 public:
  explicit VideoReader(std::unique_ptr<VideoDemuxer> demuxer);

  VideoReader(std::unique_ptr<VideoDemuxer> demuxer,
              std::vector<const VideoDecoderFactory*> decoderFactories);

  ~VideoReader() override;

  int width() const override {
    return demuxer->getFormat().width;
  }

  int height() const override {
    return demuxer->getFormat().height;
  }

 protected:
  std::shared_ptr<tgfx::ImageBuffer> onMakeBuffer(Frame targetFrame) override;

  void onReportPerformance(Performance* performance, int64_t decodingTime) override;

 private:
  enum class DecodeStatus {
    Success,
    Error,
    Stalled,
  };

  enum class DecoderType {
    Unknown,
    Hardware,
    Software,
  };

  std::mutex locker = {};
  VideoDemuxer* demuxer = nullptr;
  std::vector<const VideoDecoderFactory*> decoderFactories = {};
  float frameRate = 0.0;
  int factoryIndex = 0;
  bool preferSoftware = false;
  bool fallbackPending = false;
  VideoDecoder* videoDecoder = nullptr;
  VideoSample videoSample = {};
  std::shared_ptr<tgfx::ImageBuffer> lastBuffer = nullptr;
  bool outputEndOfStream = false;
  bool inputEndOfStream = false;
  int64_t currentDecodedTime = INT64_MIN;
  int64_t currentRenderedTime = INT64_MIN;
  std::atomic<DecoderType> lastDecoderType = DecoderType::Unknown;
  std::atomic_int64_t hardDecodingInitialTime = 0;
  std::atomic_int64_t softDecodingInitialTime = 0;

  void destroyVideoDecoder();

  bool checkVideoDecoder(int64_t deadline);

  void resetParams();

  bool sendSampleData();

  DecodeStatus decodeFrame(int64_t sampleTime, int64_t deadline);

  std::unique_ptr<VideoDecoder> makeVideoDecoder(int64_t deadline);
};
}  // namespace pag
