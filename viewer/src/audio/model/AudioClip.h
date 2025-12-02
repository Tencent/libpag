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

#include "AudioAsset.h"
#include "AudioSource.h"
#include "pag/file.h"
#include "pag/pag.h"

namespace pag {

class AudioClip {
 public:
  static void ApplyClipsToAudio(std::shared_ptr<PAGComposition> composition,
                                std::shared_ptr<AudioAsset> audio);

  AudioClip(std::shared_ptr<AudioSource> audioSource, TimeRange sourceTimeRange,
            TimeRange targetTimeRange);
  bool operator==(const AudioClip& clip) const;
  bool clearRootFile(PAGLayer* layer);
  bool applyTimeRamp(const TimeRange& source, const TimeRange& target);

  PAGFile* rootFile = nullptr;
  TimeRange sourceTimeRange = {};
  TimeRange targetTimeRange = {};
  std::shared_ptr<AudioSource> source = nullptr;

 private:
  static bool ProcessRepeatedPAGFile(std::shared_ptr<PAGFile> file,
                                     std::shared_ptr<AudioAsset> audio,
                                     std::shared_ptr<AudioSource> source,
                                     std::vector<AudioClip>& clips);
  static bool ProcessScaledPAGFile(std::shared_ptr<PAGFile> file, std::shared_ptr<AudioAsset> audio,
                                   std::shared_ptr<AudioSource> source,
                                   std::vector<AudioClip>& clips);
  static void ProcessScaledTimeRange(int64_t originalDuration, int64_t duration,
                                     int64_t audioStartTime, int64_t audioDuration,
                                     TimeRange& scaledTimeRange, std::vector<AudioClip>& clips,
                                     std::shared_ptr<AudioSource> source);

  static std::vector<AudioClip> GenerateClipsFromComposition(
      std::shared_ptr<PAGComposition> composition);
  static std::vector<AudioClip> GenerateClipsFromAudioBytes(
      std::shared_ptr<PAGComposition> composition);
  static std::vector<AudioClip> GenerateClipsFromImageLayer(std::shared_ptr<PAGImageLayer> layer);
  static std::vector<AudioClip> ExtractClipsFromImageLayer(std::shared_ptr<PAGImageLayer> layer);
  static std::vector<AudioClip> ApplyTimeRemapToClips(std::shared_ptr<PAGImageLayer> layer,
                                                      std::vector<AudioClip>& clips,
                                                      AnimatableProperty<Frame>* timeRemap);
  static void ApplyClipsToAudio(std::shared_ptr<AudioAsset> audio,
                                const std::vector<AudioClip>& clips);
};
}  // namespace pag
