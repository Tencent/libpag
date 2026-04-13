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

#include "AudioUtils.h"
#include <math.h>

namespace pag::Utils {

int64_t SampleLengthToCount(int64_t length, int channels) {
  return length / channels / 2;
}

int64_t SampleLengthToTime(int64_t length, int sampleRate, int channels) {
  int64_t count = SampleLengthToCount(length, channels);
  return static_cast<int64_t>(count * 1000000.0 / sampleRate);
}

int64_t SampleCountToLength(int64_t count, int channels) {
  return count * channels * 2;
}

int64_t SampleTimeToLength(int64_t time, int sampleRate, int channels) {
  int count = ceill(static_cast<double>(time * sampleRate) / 1000000.0);
  return SampleCountToLength(count, channels);
}

int64_t MergeSamples(const std::vector<std::shared_ptr<ByteData>>& audioSamples, uint8_t* buffer) {
  if (audioSamples.empty()) {
    return 0;
  }
  if (audioSamples.size() == 1) {
    auto& audioSample = audioSamples.front();
    memcpy(buffer, audioSample->data(), audioSample->length());
    return audioSample->length();
  }

  size_t maxLength = 0;
  for (auto& sample : audioSamples) {
    maxLength = std::max(maxLength, sample->length());
  }
  size_t sampleCount = maxLength / sizeof(int16_t);
  auto outputData = reinterpret_cast<int16_t*>(buffer);
  for (size_t index = 0; index < sampleCount; index++) {
    int32_t value = 0;
    for (auto& sample : audioSamples) {
      if (index * sizeof(int16_t) < sample->length()) {
        auto sampleData = reinterpret_cast<int16_t*>(sample->data());
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
  return static_cast<int64_t>(maxLength);
}

}  // namespace pag::Utils
