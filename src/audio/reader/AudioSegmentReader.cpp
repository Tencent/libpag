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

#include "AudioSegmentReader.h"
#include "AudioTrackSegmentReader.h"
#include "audio/utils/AudioUtils.h"

namespace pag {
std::shared_ptr<AudioSegmentReader> AudioSegmentReader::Make(
    AudioTrackSegment* segment, std::shared_ptr<PCMOutputConfig> outputConfig) {
  if (segment == nullptr) {
    return nullptr;
  }
  std::shared_ptr<AudioSegmentReader> reader = nullptr;
  if (!segment->isEmpty()) {
    reader = std::make_shared<AudioTrackSegmentReader>(segment, outputConfig);
    if (!reader->open()) {
      reader = nullptr;
    }
  }
  // 如果 segment 为空，给出一个空的 reader
  // 如果 segment 不为空且创建 reader 失败，给出一个空的 reader
  if (reader == nullptr) {
    reader = std::shared_ptr<AudioSegmentReader>(
        new AudioSegmentReader(segment, std::move(outputConfig)));
    reader->open();
  }
  return reader;
}

AudioSegmentReader::AudioSegmentReader(AudioTrackSegment* segment,
                                       std::shared_ptr<PCMOutputConfig> outputConfig)
    : segment(segment), outputConfig(std::move(outputConfig)) {
}

AudioSegmentReader::~AudioSegmentReader() {
  delete[] emptyData;
}

bool AudioSegmentReader::open() {
  if (!openInternal()) {
    return false;
  }
  startOffset = SampleTimeToLength(segment->timeMapping.target.start, outputConfig.get());
  endOffset = SampleTimeToLength(segment->timeMapping.target.end, outputConfig.get());
  currentOffset = startOffset;
  return true;
}

void AudioSegmentReader::seekTo(int64_t time) {
  currentOffset = SampleTimeToLength(time, outputConfig.get());
  seekToInternal(time);
}

SampleData AudioSegmentReader::copyNextSample() {
  if (currentOffset >= endOffset) {
    return {};
  }
  auto data = copyNextSampleInternal();
  if (data.empty()) {
    initEmptyData();
    data = SampleData(emptyData, emptyDataSize);
  }
  data.length = std::min(data.length, static_cast<size_t>(endOffset - currentOffset));
  currentOffset += static_cast<int64_t>(data.length);
  return data;
}

void AudioSegmentReader::initEmptyData() {
  if (emptyData != nullptr) {
    return;
  }
  auto length = SampleCountToLength(outputConfig->outputSamplesCount, outputConfig.get());
  auto data = new uint8_t[length];
  memset(data, 0, length);
  emptyData = data;
  emptyDataSize = length;
}
}  // namespace pag
#endif
