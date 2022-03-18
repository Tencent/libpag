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

#include "AudioSource.h"
#include "audio/utils/AudioUtils.h"
#include "pag/file.h"
#include "pag/pag.h"

namespace pag {
class AudioAsset;

class AudioClip {
 public:
  AudioClip(AudioSource audioSource, TimeRange sourceTimeRange, TimeRange targetTimeRange,
            std::vector<VolumeRange> volumeRanges)
      : audioSource(std::move(audioSource)),
        sourceTimeRange(sourceTimeRange),
        targetTimeRange(targetTimeRange),
        volumeRanges(std::move(volumeRanges)) {
  }

  static std::vector<int> DumpTracks(PAGComposition* fromComposition,
                                     AudioComposition* toAudioComposition);

  static std::vector<AudioClip> GenerateAudioClips(PAGComposition* fromComposition);

 private:
  AudioSource audioSource;
  TimeRange sourceTimeRange;
  TimeRange targetTimeRange;
  std::vector<VolumeRange> volumeRanges;
  PAGFile* rootFile = nullptr;

  static std::vector<AudioClip> GenerateAudioClips(PAGImageLayer* fromImageLayer);

  static void ShiftClipsWithLayer(std::vector<AudioClip>& audioClips, PAGLayer* layer);

  static void ClipWithTime(AudioClip& audioClip, int64_t time);

  static void ShiftClipsWithTime(AudioClip& audioClip, int64_t time);

  static std::vector<AudioClip> ProcessTimeRamp(PAGImageLayer* imageLayer,
                                                std::vector<AudioClip>& audioClips,
                                                AnimatableProperty<Frame>* timeRemap);

  static std::vector<AudioClip> ProcessImageLayer(PAGImageLayer* imageLayer);

  static void ProcessPAGAudioBytes(PAGComposition* composition, std::vector<AudioClip>& audioClips);

  static bool ProcessPAGFileRepeat(PAGFile* file, std::vector<AudioClip>& audioClips,
                                   AudioAsset* media, std::shared_ptr<ByteData>& byteData);

  static bool ProcessPAGFileScale(PAGFile* file, std::vector<AudioClip>& audioClips,
                                  AudioAsset* media, std::shared_ptr<ByteData>& byteData);

  static std::vector<int> ApplyAudioClips(AudioComposition* audioComposition,
                                          const std::vector<AudioClip>& audioClips);

  bool clearRootFile(PAGLayer* layer);

  bool applyTimeRamp(const TimeRange& source, const TimeRange& target);

  void adjustVolumeRange();
};
}  // namespace pag

#else

namespace pag {
class AudioClip {};
}  // namespace pag

#endif
