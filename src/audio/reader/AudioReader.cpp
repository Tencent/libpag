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

#include "AudioReader.h"
#include "audio/process/AudioMixer.h"
#include "audio/utils/AudioUtils.h"

namespace pag {
std::shared_ptr<AudioReader> AudioReader::Make(const std::shared_ptr<AudioAsset>& audio,
                                               std::shared_ptr<PCMOutputConfig> outputConfig) {
  auto setting = std::move(outputConfig);
  if (setting == nullptr) {
    setting = AudioReader::DefaultConfig();
  }
  std::shared_ptr<AudioReader> reader(new AudioReader(setting));
  for (auto& track : audio->tracks()) {
    reader->readers.push_back(std::make_shared<AudioTrackReader>(track, reader->_outputConfig));
  }
  reader->audio = audio;
  return reader;
}

std::shared_ptr<PCMOutputConfig> AudioReader::DefaultConfig() {
  return std::shared_ptr<PCMOutputConfig>(new PCMOutputConfig());
}

AudioReader::AudioReader(std::shared_ptr<PCMOutputConfig> outputConfig)
    : _outputConfig(std::move(outputConfig)) {
  outputBufferSize = SampleCountToLength(_outputConfig->outputSamplesCount, _outputConfig.get());
  outputBuffer = new uint8_t[outputBufferSize];
}

AudioReader::~AudioReader() {
  delete[] outputBuffer;
}

void AudioReader::seekTo(int64_t time) {
  for (const auto& reader : readers) {
    reader->seekTo(time);
  }
  currentTime = time;
  currentOutputLength = SampleTimeToLength(time, _outputConfig.get());
}

void AudioReader::onAudioChanged() {
  auto tempReaders = readers;
  readers.clear();
  for (auto& track : audio->tracks()) {
    bool found = false;
    for (auto& reader : tempReaders) {
      if (reader->track == track) {
        readers.push_back(reader);
        found = true;
        break;
      }
    }
    if (!found) {
      auto reader = std::make_shared<AudioTrackReader>(track, _outputConfig);
      if (currentTime != 0) {
        reader->seekTo(currentTime);
      }
      readers.push_back(reader);
    }
  }
}

std::shared_ptr<PAGAudioFrame> AudioReader::copyNextFrame() {
  auto time = currentTime;
  auto length = copyNextSample();
  auto frame = std::make_shared<PAGAudioFrame>();
  frame->pts = time;
  frame->duration = SampleLengthToTime(length, _outputConfig.get());
  frame->data = outputBuffer;
  frame->length = length;
  return frame;
}

int64_t AudioReader::copyNextSample() {
  std::vector<std::shared_ptr<ByteData>> byteDataVector{};
  for (const auto& reader : readers) {
    auto data = reader->copyNextSample();
    if (data == nullptr) {
      continue;
    }
    byteDataVector.push_back(data);
  }
  auto length = MergeSamples(byteDataVector, outputBuffer);
  if (length == 0) {
    memset(outputBuffer, 0, outputBufferSize);
    length = outputBufferSize;
  }
  currentOutputLength += length;
  currentTime = SampleLengthToTime(currentOutputLength, _outputConfig.get());
  return length;
}
}  // namespace pag
#endif
