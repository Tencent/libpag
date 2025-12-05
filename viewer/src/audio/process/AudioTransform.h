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

#include "audio/reader/AudioSourceReader.h"
#include "ffmovie/movie.h"
#include "sonic.h"

namespace pag {

using SmapleData = ffmovie::SampleData;
using AudioOutputConfig = ffmovie::AudioOutputConfig;

class AudioTransform {
 public:
  explicit AudioTransform(std::shared_ptr<AudioOutputConfig> outputConfig);
  ~AudioTransform();
  void setSpeed(float speed);
  void setVolume(float volume);
  void setPitch(float pitch);
  int sendAudioBytes(const SmapleData& audioData);
  int sendInputEOS();
  SampleData readAudioBytes();

 private:
  sonicStream stream = nullptr;
  std::vector<int16_t> buffer = {};
  std::shared_ptr<AudioOutputConfig> outputConfig = nullptr;
};

}  // namespace pag