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

#pragma once
#include <cstdint>
#include "pag/types.h"

namespace pag::Utils {

int64_t SampleLengthToCount(int64_t length, int channels);

int64_t SampleLengthToTime(int64_t length, int sampleRate, int channels);

int64_t SampleCountToLength(int64_t count, int channels);

int64_t SampleTimeToLength(int64_t time, int sampleRate, int channels);

int64_t MergeSamples(const std::vector<std::shared_ptr<ByteData>>& audioSamples, uint8_t* buffer);

}  // namespace pag::Utils