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

std::shared_ptr<AudioReader> AudioReader::Make(
    const std::shared_ptr<AudioAsset>& audio,
    const std::shared_ptr<AudioOutputConfig>& outputConfig) {

  auto config = outputConfig;
  if (config == nullptr) {
    config = std::make_shared<AudioOutputConfig>();
  }
  std::shared_ptr<AudioReader> reader = std::shared_ptr<AudioReader>(new AudioReader(config));
  for (auto& track : audio->getTracks()) {
    reader->readers.push_back(std::make_shared<AudioTrackReader>(track, reader->outputConfig));
  }
  reader->audio = audio;
  return reader;
}

void AudioReader::seek(int64_t time) {
  for (const auto& reader : readers) {
    reader->seek(time);
  }
  currentReadTime = time;
  currentOutputLength =
      Utils::SampleTimeToLength(time, outputConfig->sampleRate, outputConfig->channels);
  buffer.clear();
}

void AudioReader::clearBuffer() {
  for (const auto& reader : readers) {
    reader->clearBuffer();
  }
  buffer.clear();
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
  auto length = getNextSampleInternal();
  auto frame = std::make_shared<PAGAudioSample>();
  frame->time = time;
  frame->data = buffer.bytes();
  frame->length = length;
  frame->duration =
      Utils::SampleLengthToTime(length, outputConfig->sampleRate, outputConfig->channels);
  return frame;
}

std::shared_ptr<AudioOutputConfig> AudioReader::getOutputConfig() {
  return outputConfig;
}

AudioReader::AudioReader(const std::shared_ptr<AudioOutputConfig>& outputConfig)
    : outputConfig(outputConfig) {
  int64_t length =
      Utils::SampleCountToLength(outputConfig->outputSamplesCount, outputConfig->channels);
  buffer.alloc(length);
}

int64_t AudioReader::getNextSampleInternal() {
  std::vector<std::shared_ptr<ByteData>> byteDataVector = {};
  for (const auto& reader : readers) {
    auto data = reader->getNextSample();
    if (data == nullptr) {
      continue;
    }
    byteDataVector.push_back(data);
  }
  auto length = Utils::MergeSamples(byteDataVector, buffer.bytes());
  if (length == 0) {
    buffer.clear();
    length = buffer.size();
  }
  currentReadTime = Utils::SampleLengthToTime(currentOutputLength, outputConfig->sampleRate,
                                              outputConfig->channels);
  currentOutputLength += length;
  return length;
}

}  // namespace pag