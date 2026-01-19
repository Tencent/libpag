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

#include "audio/model/AudioSource.h"
#include "ffmovie/movie.h"

namespace pag {

using SampleData = ffmovie::SampleData;
using AudioOutputConfig = ffmovie::AudioOutputConfig;

class AudioSourceReader {
 public:
  explicit AudioSourceReader(std::shared_ptr<AudioSource> source, int trackID,
                             std::shared_ptr<AudioOutputConfig> outputConfig);

  bool isValid();
  void seek(int64_t time);
  SampleData getNextFrame();

 private:
  bool sendData();

  bool advance = false;
  int trackID = 0;
  int64_t startTime = 0;
  int64_t startOffset = 0;
  std::shared_ptr<AudioSource> source = nullptr;
  std::shared_ptr<AudioOutputConfig> outputConfig = nullptr;
  std::shared_ptr<ffmovie::FFAudioDemuxer> demuxer = nullptr;
  std::shared_ptr<ffmovie::FFAudioDecoder> decoder = nullptr;
};

}  // namespace pag
