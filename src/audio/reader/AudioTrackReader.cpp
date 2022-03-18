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

#ifdef FILE_MOVIE

#include "AudioTrackReader.h"

namespace pag {
AudioTrackReader::AudioTrackReader(std::shared_ptr<AudioTrack> track,
                                   std::shared_ptr<PCMOutputConfig> outputConfig)
    : track(std::move(track)), outputConfig(std::move(outputConfig)) {
  outSampleLength =
      SampleCountToLength(this->outputConfig->outputSamplesCount, this->outputConfig.get());
  sampleCacheLength =
      std::max(outSampleLength, static_cast<int64_t>(DEFAULT_OUTPUT_SAMPLE_COUNT)) * 6;
  smoothVolume = AudioSmoothVolume::Make(this->track, this->outputConfig).release();
  cacheData = new uint8_t[sampleCacheLength];
}

AudioTrackReader::~AudioTrackReader() {
  delete smoothVolume;
  delete[] cacheData;
}

void AudioTrackReader::seekTo(int64_t targetTime) {
  if (reader != nullptr && reader->segment->timeMapping.target.contains(targetTime)) {
    reader->seekTo(targetTime);
  } else {
    reader = nullptr;
  }
  readTime = targetTime;
  outputtedLength = SampleTimeToLength(readTime, outputConfig.get());
  cacheSize = 0;
  if (smoothVolume != nullptr) {
    smoothVolume->seek(targetTime);
  }
}

std::shared_ptr<ByteData> AudioTrackReader::copyNextSample() {
  auto data = copyNextSampleInternal();
  if (data == nullptr) {
    return data;
  }
  if (smoothVolume != nullptr) {
    smoothVolume->process(SampleLengthToTime(outputtedLength, outputConfig.get()), data);
  }
  return data;
}

std::shared_ptr<ByteData> AudioTrackReader::copyNextSampleInternal() {
  while (cacheSize < outSampleLength) {
    checkReader();
    if (reader == nullptr) {
      if (cacheSize > 0) {
        int64_t targetLength = cacheSize;
        auto data = new uint8_t[targetLength];
        memcpy(data, cacheData, targetLength);
        cacheSize = 0;
        outputtedLength += targetLength;
        return std::shared_ptr<ByteData>(ByteData::MakeAdopted(data, targetLength));
      }
      return nullptr;
    }
    auto data = reader->copyNextSample();
    if (data.empty()) {
      auto time = reader->segment->timeMapping.target.end - readTime;
      auto length = SampleTimeToLength(time, outputConfig.get());
      length = std::min(length, sampleCacheLength - static_cast<int64_t>(cacheSize));
      memset(cacheData + cacheSize, 0, length);
      cacheSize += length;
    } else {
      if (static_cast<int64_t>(data.length) + cacheSize > sampleCacheLength) {
        memcpy(cacheData + cacheSize, data.data, sampleCacheLength - cacheSize);
        cacheSize = static_cast<int>(sampleCacheLength);
      } else {
        memcpy(cacheData + cacheSize, data.data, data.length);
        cacheSize += data.length;
      }
    }
    readTime = SampleLengthToTime(outputtedLength + cacheSize, outputConfig.get());
  }
  auto data = new uint8_t[outSampleLength];
  memcpy(data, cacheData, outSampleLength);
  std::copy(cacheData + outSampleLength, cacheData + cacheSize, cacheData);
  cacheSize -= outSampleLength;
  outputtedLength += outSampleLength;
  return std::shared_ptr<ByteData>(ByteData::MakeAdopted(data, outSampleLength));
}

void AudioTrackReader::checkReader() {
  if (reader == nullptr || !reader->segment->timeMapping.target.contains(readTime)) {
    reader = nullptr;
    for (auto& seg : track->_segments) {
      if (seg.timeMapping.target.contains(readTime)) {
        reader = AudioSegmentReader::Make(&seg, outputConfig);
        if (reader == nullptr) {
          continue;
        }
        if (readTime - seg.timeMapping.target.start > 1000) {
          // 误差大于 1ms，需要调整
          reader->seekTo(readTime);
        }
        break;
      }
      if (seg.timeMapping.target.start > readTime) {
        break;
      }
    }
  }
}
}  // namespace pag
#endif
