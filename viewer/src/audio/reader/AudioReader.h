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

#include <list>
#include "AudioTrackReader.h"
#include "audio/model/AudioAsset.h"
#include "pag/pag.h"

namespace pag {

struct PAGAudioSample {
  int64_t time = 0;
  int64_t duration = 0;
  int64_t length = 0;
  std::shared_ptr<ByteData> data = nullptr;
};

class AudioReader {
 public:
  static std::shared_ptr<AudioReader> Make(std::shared_ptr<AudioAsset> audio,
                                           std::shared_ptr<AudioOutputConfig> outputConfig);

  void seek(int64_t time);
  void clearBuffer();
  void onAudioChanged();
  std::shared_ptr<PAGAudioSample> getNextSample();
  std::shared_ptr<AudioOutputConfig> getOutputConfig();

 private:
  explicit AudioReader(std::shared_ptr<AudioOutputConfig> outputConfig);

  std::shared_ptr<PAGAudioSample> getNextSampleInternal();
  std::shared_ptr<PAGAudioSample> mergeSamples(
      const std::vector<std::shared_ptr<ByteData>>& samples);

  int64_t currentReadTime = 0;
  int64_t currentOutputLength = 0;
  int64_t sampleLength = 0;
  std::shared_ptr<AudioAsset> audio = nullptr;
  std::shared_ptr<AudioOutputConfig> outputConfig = nullptr;
  std::list<std::shared_ptr<AudioTrackReader>> readers = {};
};

}  // namespace pag