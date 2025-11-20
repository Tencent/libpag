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

#include "AudioShifting.h"
#include "utils/AudioUtils.h"

namespace pag {

AudioShifting::AudioShifting(std::shared_ptr<AudioOutputConfig> outputConfig)
    : outputConfig(outputConfig) {
  stream = sonicCreateStream(outputConfig->sampleRate, outputConfig->channels);
  sonicSetChordPitch(stream, 1);
  buffer.resize(outputConfig->outputSamplesCount * outputConfig->channels, 0);
}

AudioShifting::~AudioShifting() {
  if (stream) {
    sonicDestroyStream(stream);
  }
}

void AudioShifting::setSpeed(float speed) {
  sonicSetSpeed(stream, speed);
}

void AudioShifting::setVolume(float volume) {
  sonicSetVolume(stream, volume);
}

void AudioShifting::setPitch(float pitch) {
  sonicSetPitch(stream, pitch);
}

int AudioShifting::sendAudioBytes(const SmapleData& audioData) {
  auto data = audioData.data;
  auto numSamples = Utils::SampleLengthToCount(audioData.length, outputConfig->channels);
  sonicWriteShortToStream(stream, reinterpret_cast<int16_t*>(data), static_cast<int>(numSamples));
  return sonicSamplesAvailable(stream);
}

int AudioShifting::sendInputEOS() {
  sonicFlushStream(stream);
  return sonicSamplesAvailable(stream);
}

SampleData AudioShifting::readAudioBytes() {
  int readSize = sonicReadShortFromStream(stream, buffer.data(), outputConfig->outputSamplesCount);
  if (readSize == 0) {
    return {};
  }
  auto length = Utils::SampleCountToLength(readSize, outputConfig->channels);
  return {reinterpret_cast<uint8_t*>(buffer.data()), static_cast<int>(length)};
}
}  // namespace pag