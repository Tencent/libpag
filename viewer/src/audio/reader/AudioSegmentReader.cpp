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

#include "AudioSegmentReader.h"
#include "utils/AudioUtils.h"

namespace pag {

std::shared_ptr<AudioSegmentReader> AudioSegmentReader::Make(
    AudioTrackSegment* segment, std::shared_ptr<AudioOutputConfig> outputConfig) {
  if (segment == nullptr) {
    return nullptr;
  }
  auto reader = std::make_shared<AudioSegmentReader>(segment, std::move(outputConfig));
  if (!reader->isValid()) {
    return nullptr;
  }

  return reader;
}

AudioSegmentReader::AudioSegmentReader(AudioTrackSegment* segment,
                                       std::shared_ptr<AudioOutputConfig> outputConfig)
    : segment(segment), outputConfig(outputConfig) {
  reader =
      std::make_shared<AudioSourceReader>(segment->source, segment->sourceTrackID, outputConfig);
  if (reader->isValid()) {
    reader->seek(segment->sourceRange.start);
  }
  speed = static_cast<float>(segment->sourceRange.duration()) /
          static_cast<float>(segment->targetRange.duration());
  startOffset = Utils::SampleTimeToLength(segment->targetRange.start, outputConfig->sampleRate,
                                          outputConfig->channels);
  endOffset = Utils::SampleTimeToLength(segment->targetRange.end, outputConfig->sampleRate,
                                        outputConfig->channels);
  currentOffset = startOffset;
}

bool AudioSegmentReader::isValid() {
  return reader != nullptr;
}

void AudioSegmentReader::seek(int64_t time) {
  currentOffset = Utils::SampleTimeToLength(time, outputConfig->sampleRate, outputConfig->channels);
  auto sourceRange = segment->sourceRange;
  auto targetRange = segment->targetRange;
  auto sourceTime = static_cast<int64_t>(
      (static_cast<double>(time - targetRange.start) * sourceRange.duration()) /
          static_cast<double>(targetRange.duration()) +
      sourceRange.start);
  inputEOS = false;
  audioTransform = nullptr;
  if (reader->isValid()) {
    reader->seek(sourceTime);
  }
}

SampleData AudioSegmentReader::getNextSample() {
  if (currentOffset >= endOffset) {
    return {};
  }
  SampleData data = getNextSampleInternal();
  if (data.empty()) {
    int64_t length =
        Utils::SampleCountToLength(outputConfig->outputSamplesCount, outputConfig->channels);
    emptyBuffer.resize(length);
    std::fill(emptyBuffer.begin(), emptyBuffer.end(), 0);
    data.data = emptyBuffer.data();
    data.length = length;
  }
  data.length = std::min(data.length, static_cast<size_t>(endOffset - currentOffset));
  currentOffset += static_cast<int64_t>(data.length);
  return data;
}

SampleData AudioSegmentReader::getNextSampleInternal() {
  if (!reader->isValid()) {
    return {};
  }
  if (speed == 1.0) {
    return reader->getNextFrame();
  }
  if (audioTransform == nullptr) {
    audioTransform = std::make_shared<AudioTransform>(outputConfig);
    audioTransform->setSpeed(speed);
  }
  int ret = 0;
  while (!inputEOS && ret <= 0) {
    auto data = reader->getNextFrame();
    if (data.empty()) {
      ret = audioTransform->sendInputEOS();
      inputEOS = true;
    } else {
      ret = audioTransform->sendAudioBytes(data);
    }
  }
  return audioTransform->readAudioBytes();
}

}  // namespace pag
