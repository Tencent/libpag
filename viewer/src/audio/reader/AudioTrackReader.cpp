/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#include "AudioTrackReader.h"
#include "utils/AudioUtils.h"

namespace pag {

static constexpr int64_t MaxOutputSampleCount = 1024;

AudioTrackReader::AudioTrackReader(std::shared_ptr<AudioTrack> track,
                                   std::shared_ptr<AudioOutputConfig> outputConfig)
    : track(track), outputConfig(outputConfig) {
  outputSampleLength =
      Utils::SampleCountToLength(outputConfig->outputSamplesCount, outputConfig->channels);
  int64_t bufferSize = std::max(outputSampleLength, MaxOutputSampleCount) * 6;
  buffer.alloc(bufferSize);
}

void AudioTrackReader::seek(int64_t time) {
  segmentReader = nullptr;
  currentReadTime = time;
  outputLength = Utils::SampleTimeToLength(time, outputConfig->sampleRate, outputConfig->channels);
  buffer.clear();
  cacheSize = 0;
}

void AudioTrackReader::clearBuffer() {
  buffer.clear();
  cacheSize = 0;
}

void AudioTrackReader::checkReader() {
  if (segmentReader == nullptr || !segmentReader->segment->targetRange.contains(currentReadTime)) {
    segmentReader = nullptr;
    for (auto& segment : track->segments) {
      if (segment.targetRange.contains(currentReadTime)) {
        segmentReader = AudioSegmentReader::Make(&segment, outputConfig);
        if (segmentReader == nullptr) {
          continue;
        }
        if (currentReadTime - segment.targetRange.start > 1000) {
          segmentReader->seek(currentReadTime);
        }
        break;
      }
      if (segment.targetRange.start > currentReadTime) {
        break;
      }
    }
  }
}

std::shared_ptr<ByteData> AudioTrackReader::getNextSample() {
  return getNextSampleInternal();
}

std::shared_ptr<ByteData> AudioTrackReader::getNextSampleInternal() {
  while (cacheSize < outputSampleLength) {
    checkReader();
    if (segmentReader == nullptr) {
      if (cacheSize > 0) {
        int64_t length = cacheSize;
        outputLength += length;
        auto data = ByteData::MakeCopy(buffer.data(), length);
        cacheSize = 0;
        return std::move(data);
      }
      return nullptr;
    }
    auto data = segmentReader->getNextSample();
    if (data.empty()) {
      auto time = segmentReader->segment->targetRange.end - currentReadTime;
      if (time <= 0) {
        currentReadTime = std::max(currentReadTime, segmentReader->segment->targetRange.end + 1);
        return nullptr;
      }
      auto length =
          Utils::SampleTimeToLength(time, outputConfig->sampleRate, outputConfig->channels);
      length = std::min(length, static_cast<int64_t>(buffer.size() - cacheSize));
      std::memset(buffer.bytes() + cacheSize, 0, length);
      cacheSize += length;
    } else {
      int64_t tmpSize =
          (data.length + cacheSize > buffer.size()) ? buffer.size() - cacheSize : data.length;
      std::memcpy(buffer.bytes() + cacheSize, data.data, tmpSize);
      cacheSize += tmpSize;
    }
    currentReadTime = Utils::SampleLengthToTime(outputLength + cacheSize, outputConfig->sampleRate,
                                                outputConfig->channels);
  }
  auto theData = ByteData::MakeCopy(buffer.data(), outputSampleLength);
  std::shared_ptr<ByteData> data = std::move(theData);
  std::memcpy(buffer.bytes(), buffer.bytes() + outputSampleLength, cacheSize - outputSampleLength);
  cacheSize -= outputSampleLength;
  outputLength += outputSampleLength;
  return data;
}

}  // namespace pag
