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

#pragma once
#ifdef FILE_MOVIE

#include <vector>
#include "audio/model/AudioCompositionTrack.h"
#include "audio/utils/AudioUtils.h"

namespace pag {
struct VolumeControlInfo {
  int64_t startTime = 0;
  int64_t endTime = 0;
  float startVolume = 0.0f;
  float targetVolume = 0.0f;
  int totalFrameNumber = 0;
};

class AudioSmoothVolume {
 public:
  static std::unique_ptr<AudioSmoothVolume> Make(const std::shared_ptr<AudioTrack>& track,
                                                 std::shared_ptr<PCMOutputConfig> pcmOutputConfig);

  void process(int64_t pts, const std::shared_ptr<ByteData>& data);

  void seek(int64_t time);

 private:
  std::shared_ptr<AudioCompositionTrack> track{};
  std::shared_ptr<PCMOutputConfig> pcmOutputConfig;
  std::vector<VolumeControlInfo> volumeControlInfo{};
  int currentVolumeIndex = 0;
  float currentAudioVolume = 1.0f;
  bool updateGainGap = true;
  float volumeGapInFrame = 0.0f;

  AudioSmoothVolume(std::shared_ptr<AudioCompositionTrack> track,
                    std::shared_ptr<PCMOutputConfig> pcmOutputConfig);

  void calcSmoothVolumeRange(int64_t inputPts, float& currentGain, float& targetGain);

  static void smoothVolumeProcessing(uint8_t* audioBuffer, int byteSize, int channelNumber,
                                     float currentGain, float targetGain);

  static void gainApplyProcess(float gain, uint8_t* buffer, int byteSize);
};
}  // namespace pag

#endif
