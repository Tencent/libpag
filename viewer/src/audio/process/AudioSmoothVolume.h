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

#include <vector>
#include "AudioShifting.h"
#include "audio/model/AudioTrack.h"

namespace pag {

struct VolumeControlInfo {
  float startVolume = 0.0f;
  float targetVolume = 0.0f;
  int64_t startTime = 0;
  int64_t endTime = 0;
  int totalFrameNumber = 0;
};

class AudioSmoothVolume {
 public:
  static std::shared_ptr<AudioSmoothVolume> Make(
      const std::shared_ptr<AudioTrack>& track,
      const std::shared_ptr<AudioOutputConfig>& outputConfig);

  void process(int64_t time, const std::shared_ptr<ByteData>& data);
  void seek(int64_t time);

 private:
  static void smoothVolumeProcessing(uint8_t* audioBuffer, int byteSize, int channelNumber,
                                     float currentGain, float targetGain);
  static void gainApplyProcess(float gain, uint8_t* buffer, int byteSize);

  AudioSmoothVolume(const std::shared_ptr<AudioTrack>& track,
                    const std::shared_ptr<AudioOutputConfig>& outputConfig);
  void calcSmoothVolumeRange(int64_t inputPts, float& currentGain, float& targetGain);

  bool updateGainGap = true;
  int currentVolumeIndex = 0;
  float currentVolume = 1.0f;
  float volumeGapInFrame = 0.0f;
  std::vector<VolumeControlInfo> volumeControlInfos = {};
  std::shared_ptr<AudioTrack> track = nullptr;
  std::shared_ptr<AudioOutputConfig> outputConfig = nullptr;
};

}  // namespace pag