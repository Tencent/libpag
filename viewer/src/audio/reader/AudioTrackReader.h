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

#include <tgfx/core/Buffer.h>
#include "AudioSegmentReader.h"
#include "audio/model/AudioTrack.h"
#include "pag/pag.h"

namespace pag {

class AudioTrackReader {
 public:
  AudioTrackReader(std::shared_ptr<AudioTrack> track,
                   std::shared_ptr<AudioOutputConfig> outputConfig);

  void seek(int64_t time);
  void clearBuffer();
  std::shared_ptr<ByteData> getNextSample();

 private:
  void checkReader();
  std::shared_ptr<ByteData> getNextSampleInternal();

  int64_t cacheSize = 0;           // The size of the buffer that has been used
  int64_t outputLength = 0;        // The size of the buffer that has been output
  int64_t currentReadTime = 0;     // The current read position in the audio track
  int64_t outputSampleLength = 0;  // The length of the output sample
  tgfx::Buffer buffer = {};
  std::shared_ptr<AudioTrack> track = nullptr;
  std::shared_ptr<AudioOutputConfig> outputConfig = nullptr;
  std::shared_ptr<AudioSegmentReader> segmentReader = nullptr;

  friend class AudioReader;
};

}  // namespace pag
