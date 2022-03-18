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

#include "AudioMixer.h"
#include <algorithm>
#include <climits>
#include <vector>

namespace pag {
int64_t MergeSamples(const std::vector<std::shared_ptr<ByteData>>& audioSamples, uint8_t* output) {
  if (audioSamples.empty()) {
    return 0;
  }
  size_t maxLength = 0;
  for (const auto& sample : audioSamples) {
    maxLength = std::max(maxLength, sample->length());
  }
  int64_t sampleCount = static_cast<int64_t>(maxLength + 1) / 2;
  auto data = reinterpret_cast<int16_t*>(output);
  for (size_t i = 0; i < maxLength; i += 2) {
    int32_t value = 0;
    for (const auto& sample : audioSamples) {
      if (i < sample->length()) {
        int16_t sampleValue = (reinterpret_cast<int16_t*>(sample->data()))[i / 2];
        value += sampleValue;
      }
    }
    if (value > SHRT_MAX) {
      value = SHRT_MAX;
    } else if (value < SHRT_MIN) {
      value = SHRT_MIN;
    }
    data[i / 2] = value;
  }
  return sampleCount * 2;
}
}  // namespace pag
#endif
