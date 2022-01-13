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

#include <memory>
#include <vector>

namespace pag {
class SampleData {
 public:
  /**
   * Compression-encoded data after unpacking.
   */
  uint8_t* data = nullptr;
  /**
   * The size of each compression-encoded data.
   */
  size_t length = 0;

  SampleData() = default;

  SampleData(uint8_t* data, int64_t length) : data(data), length(length) {
  }

  bool empty() const {
    return data == nullptr || length == 0;
  }
};

class PTSDetail {
 public:
  PTSDetail(int64_t duration, std::vector<int64_t> ptsVector, std::vector<int> keyframeIndexVector)
      : duration(duration),
        ptsVector(std::move(ptsVector)),
        keyframeIndexVector(std::move(keyframeIndexVector)) {
  }
  int64_t duration = 0;
  std::vector<int64_t> ptsVector;
  std::vector<int> keyframeIndexVector;

  int findKeyframeIndex(int64_t atTime) const;
  int64_t getKeyframeTime(int withKeyframeIndex) const;
  int64_t getSampleTimeAt(int64_t targetTime) const;
  int64_t getNextSampleTimeAt(int64_t targetTime);

 private:
  int findFrameIndex(int64_t targetTime) const;
};

class MediaDemuxer {
 public:
  virtual ~MediaDemuxer() = default;

  virtual int64_t getSampleTime() = 0;

  virtual bool advance() = 0;

  virtual SampleData readSampleData() = 0;

  int64_t getSampleTimeAt(int64_t targetTime);

  int64_t getNextSampleTimeAt(int64_t targetTime);

  bool trySeek(int64_t targetTime, int64_t currentTime);

  void resetParams();

 protected:
  virtual void seekTo(int64_t timeUs) = 0;

  virtual std::shared_ptr<PTSDetail> createPTSDetail() = 0;

  std::shared_ptr<PTSDetail> ptsDetail();

  void afterAdvance(bool isKeyframe);

 private:
  std::shared_ptr<PTSDetail> ptsDetail_ = nullptr;
  int64_t maxPendingTime = INT64_MIN;
  int currentKeyframeIndex = -1;
};
}  // namespace pag
