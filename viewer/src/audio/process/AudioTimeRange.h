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

#include <pag/types.h>
#include <memory>

namespace pag {

class AudioClip;
class AudioSource;

void ProcessAudioBeforeScaledRegion(int64_t fileDuration, int64_t audioStartTime,
                                    int64_t audioDuration, std::vector<AudioClip>& clips,
                                    std::shared_ptr<AudioSource> source);

void ProcessAudioAfterScaledRegion(int64_t originFileDuration, int64_t fileDuration,
                                   int64_t audioStartTime, int64_t audioDuration,
                                   TimeRange& scaledTimeRange, std::vector<AudioClip>& clips,
                                   std::shared_ptr<AudioSource> source);

void ProcessAudioWrapsScaledRegion(int64_t originFileDuration, int64_t fileDuration,
                                   int64_t audioStartTime, int64_t audioDuration,
                                   TimeRange& scaledTimeRange, std::vector<AudioClip>& clips,
                                   std::shared_ptr<AudioSource> source);

void ProcessAudioInsideScaledRegion(int64_t originFileDuration, int64_t fileDuration,
                                    int64_t audioStartTime, int64_t audioDuration,
                                    TimeRange& scaledTimeRange, std::vector<AudioClip>& clips,
                                    std::shared_ptr<AudioSource> source);

void ProcessAudioOverlapsScaledStart(int64_t originFileDuration, int64_t fileDuration,
                                     int64_t audioStartTime, int64_t audioDuration,
                                     TimeRange& scaledTimeRange, std::vector<AudioClip>& clips,
                                     std::shared_ptr<AudioSource> source);

void ProcessAudioOverlapsScaledEnd(int64_t originFileDuration, int64_t fileDuration,
                                   int64_t audioStartTime, int64_t audioDuration,
                                   TimeRange& scaledTimeRange, std::vector<AudioClip>& clips,
                                   std::shared_ptr<AudioSource> source);

}  // namespace pag
