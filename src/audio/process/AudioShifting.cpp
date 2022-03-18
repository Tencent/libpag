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

#include "AudioShifting.h"

namespace pag {
AudioShifting::AudioShifting(std::shared_ptr<PCMOutputConfig> pcmOutputConfig)
    : pcmOutputConfig(std::move(pcmOutputConfig)) {
  stream = sonicCreateStream(this->pcmOutputConfig->sampleRate, this->pcmOutputConfig->channels);
  sonicSetChordPitch(stream, 1);
}

AudioShifting::~AudioShifting() {
  delete[] outPCMBuffer;
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

int AudioShifting::sendAudioBytes(const SampleData& audioData) {
  auto data = audioData.data;
  auto numSamples = SampleLengthToCount(audioData.length, pcmOutputConfig.get());
  sonicWriteShortToStream(stream, reinterpret_cast<int16_t*>(data), static_cast<int>(numSamples));
  return sonicSamplesAvailable(stream);
}

int AudioShifting::sendInputEOS() {
  sonicFlushStream(stream);
  return sonicSamplesAvailable(stream);
}

SampleData AudioShifting::readAudioBytes() {
  if (outPCMBuffer == nullptr) {
    outPCMBuffer = new int16_t[pcmOutputConfig->outputSamplesCount * pcmOutputConfig->channels];
  }
  int samplesWritten =
      sonicReadShortFromStream(stream, outPCMBuffer, pcmOutputConfig->outputSamplesCount);
  if (samplesWritten == 0) {
    return {};
  }
  auto lineSize = SampleCountToLength(samplesWritten, pcmOutputConfig.get());
  return {reinterpret_cast<uint8_t*>(outPCMBuffer), static_cast<int>(lineSize)};
}
}  // namespace pag
#endif
