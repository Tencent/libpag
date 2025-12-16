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

#include "AudioTimeRange.h"
#include "audio/model/AudioClip.h"

namespace pag {

void ProcessAudioBeforeScaledRegion(int64_t fileDuration, int64_t audioStartTime,
                                    int64_t audioDuration, std::vector<AudioClip>& clips,
                                    std::shared_ptr<AudioSource> source) {
  if (fileDuration > audioStartTime) {
    int64_t duration = std::min(audioDuration, fileDuration - audioStartTime);
    if (duration > 0) {
      auto sourceTimeRange = TimeRange{0, duration};
      auto targetTimeRange = TimeRange{audioStartTime, audioStartTime + duration};
      auto clip = AudioClip(source, sourceTimeRange, targetTimeRange);
      clips.push_back(clip);
    }
  }
}

void ProcessAudioAfterScaledRegion(int64_t originFileDuration, int64_t fileDuration,
                                   int64_t audioStartTime, int64_t audioDuration,
                                   TimeRange& scaledTimeRange, std::vector<AudioClip>& clips,
                                   std::shared_ptr<AudioSource> source) {
  auto minDuration = originFileDuration - scaledTimeRange.duration();

  auto targetStart = audioStartTime + std::max(fileDuration, minDuration) - originFileDuration;

  if (fileDuration > targetStart) {
    auto duration = std::min(audioDuration, fileDuration - targetStart);
    auto sourceTimeRange = TimeRange{0, duration};
    auto targetTimeRange = TimeRange{targetStart, targetStart + duration};
    auto clip = AudioClip(std::move(source), sourceTimeRange, targetTimeRange);
    clips.push_back(clip);
  }
}

void ProcessAudioWrapsScaledRegion(int64_t originFileDuration, int64_t fileDuration,
                                   int64_t audioStartTime, int64_t audioDuration,
                                   TimeRange& scaledTimeRange, std::vector<AudioClip>& clips,
                                   std::shared_ptr<AudioSource> source) {
  auto audioEndTime = audioStartTime + audioDuration;

  if (fileDuration > audioStartTime) {
    auto endTime = std::min(scaledTimeRange.start, fileDuration);
    auto duration = std::min(audioDuration, endTime - audioStartTime);
    auto sourceTimeRange = TimeRange{0, duration};
    auto targetTimeRange = TimeRange{audioStartTime, audioStartTime + duration};
    auto clip = AudioClip(source, sourceTimeRange, targetTimeRange);
    clips.push_back(clip);
  }

  auto minDuration = originFileDuration - scaledTimeRange.duration();
  if (fileDuration > minDuration) {
    auto sourceStart = scaledTimeRange.start - audioStartTime;
    auto sourceEnd = sourceStart + scaledTimeRange.duration();
    auto targetStart = scaledTimeRange.start;
    auto targetEnd = targetStart + fileDuration - minDuration;
    auto sourceTimeRange = TimeRange{sourceStart, sourceEnd};
    auto targetTimeRange = TimeRange{targetStart, targetEnd};
    auto clip = AudioClip(source, sourceTimeRange, targetTimeRange);
    clips.push_back(clip);
  }

  auto targetStart = scaledTimeRange.end + std::max(fileDuration, minDuration) - originFileDuration;
  if (fileDuration > targetStart) {
    auto duration = std::min(audioEndTime - scaledTimeRange.end, fileDuration - targetStart);
    auto sourceStart = scaledTimeRange.end - audioStartTime;
    auto sourceTimeRange = TimeRange{sourceStart, sourceStart + duration};
    auto targetTimeRange = TimeRange{targetStart, targetStart + duration};
    auto clip = AudioClip(source, sourceTimeRange, targetTimeRange);
    clips.push_back(clip);
  }
}

void ProcessAudioInsideScaledRegion(int64_t originFileDuration, int64_t fileDuration,
                                    int64_t audioStartTime, int64_t audioDuration,
                                    TimeRange& scaledTimeRange, std::vector<AudioClip>& clips,
                                    std::shared_ptr<AudioSource> source) {
  auto minDuration = originFileDuration - scaledTimeRange.duration();

  if (fileDuration > minDuration) {
    auto newScaledDuration = fileDuration - minDuration;
    auto targetStart = scaledTimeRange.start + (audioStartTime - scaledTimeRange.start) *
                                                   newScaledDuration / scaledTimeRange.duration();
    auto targetEnd = targetStart + audioDuration * newScaledDuration / scaledTimeRange.duration();

    auto sourceTimeRange = TimeRange{0, audioDuration};
    auto targetTimeRange = TimeRange{targetStart, targetEnd};
    auto clip = AudioClip(std::move(source), sourceTimeRange, targetTimeRange);
    clips.push_back(clip);
  }
}

void ProcessAudioOverlapsScaledStart(int64_t originFileDuration, int64_t fileDuration,
                                     int64_t audioStartTime, int64_t audioDuration,
                                     TimeRange& scaledTimeRange, std::vector<AudioClip>& clips,
                                     std::shared_ptr<AudioSource> source) {
  auto audioEndTime = audioStartTime + audioDuration;

  if (fileDuration > audioStartTime) {
    auto duration = std::min(fileDuration, scaledTimeRange.start) - audioStartTime;
    auto targetTimeRange = TimeRange{audioStartTime, audioStartTime + duration};
    auto clip = AudioClip(source, TimeRange{0, duration}, targetTimeRange);
    clips.push_back(clip);

    auto minDuration = originFileDuration - scaledTimeRange.duration();
    if (fileDuration > minDuration) {
      auto targetStart = scaledTimeRange.start;
      auto targetEnd = targetStart + (audioEndTime - scaledTimeRange.start) *
                                         (fileDuration - minDuration) / scaledTimeRange.duration();
      auto sourceTimeRange = TimeRange{duration, audioDuration};
      targetTimeRange = TimeRange{targetStart, targetEnd};
      clip = AudioClip(source, sourceTimeRange, targetTimeRange);
      clips.push_back(clip);
    }
  }
}

void ProcessAudioOverlapsScaledEnd(int64_t originFileDuration, int64_t fileDuration,
                                   int64_t audioStartTime, int64_t audioDuration,
                                   TimeRange& scaledTimeRange, std::vector<AudioClip>& clips,
                                   std::shared_ptr<AudioSource> source) {
  auto audioEndTime = audioStartTime + audioDuration;

  if (fileDuration > scaledTimeRange.start) {
    auto minDuration = originFileDuration - scaledTimeRange.duration();
    auto offset = std::max(minDuration, fileDuration) - originFileDuration;
    auto scaledTargetEnd = scaledTimeRange.end + offset;

    if (fileDuration > minDuration) {
      auto targetStart = scaledTargetEnd - (scaledTimeRange.end - audioStartTime) *
                                               (fileDuration - minDuration) /
                                               scaledTimeRange.duration();
      auto sourceTimeRange = TimeRange{0, scaledTimeRange.end - audioStartTime};
      auto targetTimeRange = TimeRange{targetStart, scaledTargetEnd};
      auto clip = AudioClip(source, sourceTimeRange, targetTimeRange);
      clips.push_back(clip);
    }

    auto audioTargetEnd = audioEndTime + offset;
    auto sourceTimeRange = TimeRange{scaledTimeRange.end - audioStartTime, audioDuration};
    auto targetTimeRange = TimeRange{scaledTargetEnd, audioTargetEnd};
    auto clip = AudioClip(source, sourceTimeRange, targetTimeRange);
    clips.push_back(clip);
  }
}

}  // namespace pag