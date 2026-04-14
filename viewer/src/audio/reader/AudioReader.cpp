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

#include "AudioReader.h"
#include <QDebug>
#include "utils/AudioUtils.h"

namespace pag {

std::shared_ptr<AudioReader> AudioReader::Make(std::shared_ptr<AudioAsset> audio,
                                               std::shared_ptr<AudioOutputConfig> outputConfig) {

  auto config = std::move(outputConfig);
  if (config == nullptr) {
    config = std::make_shared<AudioOutputConfig>();
  }
  std::shared_ptr<AudioReader> reader = std::shared_ptr<AudioReader>(new AudioReader(config));
  for (auto& track : audio->getTracks()) {
    reader->readers.push_back(std::make_shared<AudioTrackReader>(track, reader->outputConfig));
  }
  reader->audio = std::move(audio);
  return reader;
}

void AudioReader::seek(int64_t time) {
  for (const auto& reader : readers) {
    reader->seek(time);
  }
  currentReadTime = time;
  currentOutputLength =
      Utils::SampleTimeToLength(time, outputConfig->sampleRate, outputConfig->channels);
}

void AudioReader::clearBuffer() {
  for (const auto& reader : readers) {
    reader->clearBuffer();
  }
}

void AudioReader::onAudioChanged() {
  auto tempReaders = readers;
  readers.clear();
  for (auto& track : audio->getTracks()) {
    bool found = false;
    for (auto& reader : tempReaders) {
      if (reader->track == track) {
        readers.push_back(reader);
        found = true;
        break;
      }
    }
    if (!found) {
      auto reader = std::make_shared<AudioTrackReader>(track, outputConfig);
      if (currentReadTime != 0) {
        reader->seek(currentReadTime);
      }
      readers.push_back(reader);
    }
  }
}

std::shared_ptr<PAGAudioSample> AudioReader::getNextSample() {
  auto time = currentReadTime;
  auto sample = getNextSampleInternal();
  sample->time = time;
  sample->duration =
      Utils::SampleLengthToTime(sample->length, outputConfig->sampleRate, outputConfig->channels);
  return sample;
}

std::shared_ptr<AudioOutputConfig> AudioReader::getOutputConfig() {
  return outputConfig;
}

AudioReader::AudioReader(std::shared_ptr<AudioOutputConfig> outputConfig)
    : outputConfig(outputConfig) {
  sampleLength =
      Utils::SampleCountToLength(outputConfig->outputSamplesCount, outputConfig->channels);
}

std::shared_ptr<PAGAudioSample> AudioReader::getNextSampleInternal() {
  std::vector<std::shared_ptr<ByteData>> byteDataVector = {};
  for (const auto& reader : readers) {
    auto data = reader->getNextSample();
    if (data == nullptr) {
      continue;
    }
    byteDataVector.push_back(data);
  }
  auto sample = mergeSamples(byteDataVector);
  currentReadTime = Utils::SampleLengthToTime(currentOutputLength, outputConfig->sampleRate,
                                              outputConfig->channels);
  currentOutputLength += sample->length;
  return sample;
}

std::shared_ptr<PAGAudioSample> AudioReader::mergeSamples(
    const std::vector<std::shared_ptr<ByteData>>& samples) {
  auto mergedSample = std::make_shared<PAGAudioSample>();
  if (samples.empty()) {
    mergedSample->length = sampleLength;
    mergedSample->data = ByteData::Make(sampleLength);
    std::memset(mergedSample->data->data(), 0, sampleLength);
    return mergedSample;
  }

  if (samples.size() == 1) {
    mergedSample->length = samples.front()->length();
    mergedSample->data = samples.front();
    return mergedSample;
  }

  size_t maxLength = 0;
  for (const auto& sample : samples) {
    maxLength = std::max(maxLength, sample->length());
  }

  mergedSample->length = maxLength;
  mergedSample->data = ByteData::Make(maxLength);
  auto outputData = reinterpret_cast<int16_t*>(mergedSample->data->data());
  size_t sampleCount = maxLength / sizeof(int16_t);
  for (size_t index = 0; index < sampleCount; index++) {
    int32_t value = 0;
    for (const auto& sample : samples) {
      size_t sampleByteLength = sample->length();
      if (index * sizeof(int16_t) < sampleByteLength) {
        auto sampleData = reinterpret_cast<const int16_t*>(sample->data());
        value += sampleData[index];
      }
    }
    if (value > SHRT_MAX) {
      value = SHRT_MAX;
    } else if (value < SHRT_MIN) {
      value = SHRT_MIN;
    }
    outputData[index] = static_cast<int16_t>(value);
  }
  return mergedSample;
}

}  // namespace pag
