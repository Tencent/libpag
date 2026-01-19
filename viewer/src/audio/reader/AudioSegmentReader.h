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
#include <memory>
#include "AudioSourceReader.h"
#include "audio/model/AudioTrackSegment.h"
#include "audio/process/AudioTransform.h"

namespace pag {

class AudioSegmentReader {
 public:
  static std::shared_ptr<AudioSegmentReader> Make(AudioTrackSegment* segment,
                                                  std::shared_ptr<AudioOutputConfig> outputConfig);

  AudioSegmentReader(AudioTrackSegment* segment, std::shared_ptr<AudioOutputConfig> outputConfig);
  bool isValid();
  void seek(int64_t time);
  SampleData getNextSample();

 private:
  SampleData getNextSampleInternal();

  bool inputEOS = false;
  float speed = 1.0f;
  int64_t startOffset = 0;
  int64_t endOffset = 0;
  int64_t currentOffset = 0;
  AudioTrackSegment* segment = nullptr;
  std::vector<uint8_t> emptyBuffer = {};
  std::shared_ptr<AudioTransform> audioTransform = nullptr;
  std::shared_ptr<AudioSourceReader> reader = nullptr;
  std::shared_ptr<AudioOutputConfig> outputConfig = nullptr;

  friend class AudioTrackReader;
};

}  // namespace pag
