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

#ifdef FILE_MOVIE

#include "AudioClip.h"
#include "audio/model/AudioAsset.h"
#include "audio/model/AudioComposition.h"
#include "audio/utils/AudioUtils.h"
#include "base/utils/TimeUtil.h"
#include "rendering/editing/PAGImageHolder.h"

namespace pag {
bool AudioClip::ProcessPAGFileRepeat(PAGFile* file, std::vector<AudioClip>& audioClips,
                                     AudioAsset* media, std::shared_ptr<ByteData>& byteData) {
  if (file->_timeStretchMode != PAGTimeStretchMode::Repeat) {
    return false;
  }
  auto originFileDuration = FrameToTime(file->file->duration(), file->file->frameRate());
  auto fileDuration = file->durationInternal();
  if (fileDuration <= originFileDuration) {
    return false;
  }
  auto audioStartTime = file->audioStartTime();
  auto originAudioDuration = originFileDuration - audioStartTime;
  auto startTime = audioStartTime;
  while (fileDuration > startTime) {
    auto duration = std::min(fileDuration - startTime, originAudioDuration);
    duration = std::min(media->duration(), duration);
    auto sourceTimeRange = TimeRange{0, duration};
    auto targetTimeRange = TimeRange{startTime, startTime + duration};
    AudioClip clip(AudioSource(byteData), sourceTimeRange, targetTimeRange, {});
    audioClips.push_back(clip);
    startTime += originFileDuration;
  }
  return true;
}

void ProcessLeftIntersect(int64_t originFileDuration, int64_t fileDuration, int64_t audioStartTime,
                          int64_t audioDuration, TimeRange& scaledTimeRange,
                          std::vector<AudioClip>& audioClips, std::shared_ptr<ByteData>& byteData) {
  //
  // {}: scaledTimeRange
  // []: audioTimeRange
  //
  // |___[___{___]___}______|
  // |       |      /      /
  // |       |     /      /
  // |       |    /      /
  // |___[___{_]_}______|
  // |       |  /      /
  // |       | /      /
  // |       |/      /
  // |___[___]______|
  //
  auto audioEndTime = audioStartTime + audioDuration;
  if (fileDuration > audioStartTime) {
    auto duration = std::min(fileDuration, scaledTimeRange.start) - audioStartTime;
    audioClips.push_back(AudioClip(AudioSource(byteData), TimeRange{0, duration},
                                   TimeRange{audioStartTime, audioStartTime + duration}, {}));
    auto minDuration = originFileDuration - scaledTimeRange.duration();
    if (fileDuration > minDuration) {
      auto targetStart = scaledTimeRange.start;
      auto targetEnd = targetStart + (audioEndTime - scaledTimeRange.start) *
                                         (fileDuration - minDuration) / scaledTimeRange.duration();
      auto sourceTimeRange = TimeRange{duration, audioDuration};
      auto targetTimeRange = TimeRange{targetStart, targetEnd};
      audioClips.push_back(AudioClip(AudioSource(byteData), sourceTimeRange, targetTimeRange, {}));
    }
  }
}

void ProcessRightIntersect(int64_t originFileDuration, int64_t fileDuration, int64_t audioStartTime,
                           int64_t audioDuration, TimeRange& scaledTimeRange,
                           std::vector<AudioClip>& audioClips,
                           std::shared_ptr<ByteData>& byteData) {
  //
  // {}: scaledTimeRange
  // []: audioTimeRange
  //
  // |______{___[___}___]___|
  // |      |      /       /
  // |      |     /       /
  // |      |    /       /
  // |______{_[_}___]___|
  // |      |  /       /
  // |      | /       /
  // |      |/       /
  // |______[___]___|
  //
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
      audioClips.push_back(AudioClip(AudioSource(byteData), sourceTimeRange, targetTimeRange, {}));
    }
    auto audioTargetEnd = audioEndTime + offset;
    auto sourceTimeRange = TimeRange{scaledTimeRange.end - audioStartTime, audioDuration};
    auto targetTimeRange = TimeRange{scaledTargetEnd, audioTargetEnd};
    audioClips.push_back(AudioClip(AudioSource(byteData), sourceTimeRange, targetTimeRange, {}));
  }
}

void ProcessScaledContainAudio(int64_t originFileDuration, int64_t fileDuration,
                               int64_t audioStartTime, int64_t audioDuration,
                               TimeRange& scaledTimeRange, std::vector<AudioClip>& audioClips,
                               std::shared_ptr<ByteData>& byteData) {
  //
  // {}: scaledTimeRange
  // []: audioTimeRange
  //
  // |___[___{______}___]___|
  // |       |     /       /
  // |       |    /       /
  // |___[___{___}___]___|
  // |       |  /       /
  // |       | /       /
  // |       |/       /
  // |___[___|___]___|
  //
  auto audioEndTime = audioStartTime + audioDuration;
  if (fileDuration > audioStartTime) {
    auto endTime = std::min(scaledTimeRange.start, fileDuration);
    auto duration = std::min(audioDuration, endTime - audioStartTime);
    auto sourceTimeRange = TimeRange{0, duration};
    auto targetTimeRange = TimeRange{audioStartTime, audioStartTime + duration};
    audioClips.push_back(AudioClip(AudioSource(byteData), sourceTimeRange, targetTimeRange, {}));
  }
  auto minDuration = originFileDuration - scaledTimeRange.duration();
  if (fileDuration > minDuration) {
    auto sourceStart = scaledTimeRange.start - audioStartTime;
    auto sourceEnd = sourceStart + scaledTimeRange.duration();
    auto targetStart = scaledTimeRange.start;
    auto targetEnd = targetStart + fileDuration - minDuration;
    auto sourceTimeRange = TimeRange{sourceStart, sourceEnd};
    auto targetTimeRange = TimeRange{targetStart, targetEnd};
    audioClips.push_back(AudioClip(AudioSource(byteData), sourceTimeRange, targetTimeRange, {}));
  }
  auto targetStart = scaledTimeRange.end + std::max(fileDuration, minDuration) - originFileDuration;
  if (fileDuration > targetStart) {
    auto duration = std::min(audioEndTime - scaledTimeRange.end, fileDuration - targetStart);
    auto sourceStart = scaledTimeRange.end - audioStartTime;
    auto sourceTimeRange = TimeRange{sourceStart, sourceStart + duration};
    auto targetTimeRange = TimeRange{targetStart, targetStart + duration};
    audioClips.push_back(AudioClip(AudioSource(byteData), sourceTimeRange, targetTimeRange, {}));
  }
}

void ProcessLeftOutside(int64_t fileDuration, int64_t audioStartTime, int64_t audioDuration,
                        std::vector<AudioClip>& audioClips, std::shared_ptr<ByteData>& byteData) {
  //
  // {}: scaledTimeRange
  // []: audioTimeRange
  //
  // |__[__]__{______}______|
  // |        |     /      /
  // |        |    /      /
  // |__[__]__{___}______|
  // |        |  /      /
  // |        | /      /
  // |        |/      /
  // |__[__]__|______|
  //
  if (fileDuration > audioStartTime) {
    auto duration = std::min(audioDuration, fileDuration - audioStartTime);
    if (duration > 0) {
      auto sourceTimeRange = TimeRange{0, duration};
      auto targetTimeRange = TimeRange{audioStartTime, audioStartTime + duration};
      audioClips.push_back(AudioClip(AudioSource(byteData), sourceTimeRange, targetTimeRange, {}));
    }
  }
}

void ProcessRightOutside(int64_t originFileDuration, int64_t fileDuration, int64_t audioStartTime,
                         int64_t audioDuration, TimeRange& scaledTimeRange,
                         std::vector<AudioClip>& audioClips, std::shared_ptr<ByteData>& byteData) {
  //
  // {}: scaledTimeRange
  // []: audioTimeRange
  //
  // |______{______}__[__]__|
  // |      |     /        /
  // |      |    /        /
  // |______{___}__[__]__|
  // |      |  /        /
  // |      | /        /
  // |      |/        /
  // |______|__[__]__|
  //
  auto minDuration = originFileDuration - scaledTimeRange.duration();
  auto targetStart = audioStartTime + std::max(fileDuration, minDuration) - originFileDuration;
  if (fileDuration > targetStart) {
    auto duration = std::min(audioDuration, fileDuration - targetStart);
    auto sourceTimeRange = TimeRange{0, duration};
    auto targetTimeRange = TimeRange{targetStart, targetStart + duration};
    audioClips.push_back(AudioClip(AudioSource(byteData), sourceTimeRange, targetTimeRange, {}));
  }
}

void ProcessAudioContainScaled(int64_t originFileDuration, int64_t fileDuration,
                               int64_t audioStartTime, int64_t audioDuration,
                               TimeRange& scaledTimeRange, std::vector<AudioClip>& audioClips,
                               std::shared_ptr<ByteData>& byteData) {
  //
  // {}: scaledTimeRange
  // []: audioTimeRange
  //
  // |______{__[__]__}______|
  // |      |       /      /
  // |      |      /      /
  // |______{_[_]_}______|
  // |      |    /      /
  // |      |   /      /
  // |      |  /      /
  // |      | /      /
  // |      |/      /
  // |______|______|
  //
  auto minDuration = originFileDuration - scaledTimeRange.duration();
  if (fileDuration > minDuration) {
    auto targetStart = scaledTimeRange.start + (audioStartTime - scaledTimeRange.start) *
                                                   (fileDuration - minDuration) /
                                                   scaledTimeRange.duration();
    auto targetEnd =
        targetStart + audioDuration * (fileDuration - minDuration) / scaledTimeRange.duration();
    auto sourceTimeRange = TimeRange{0, audioDuration};
    auto targetTimeRange = TimeRange{targetStart, targetEnd};
    audioClips.push_back(AudioClip(AudioSource(byteData), sourceTimeRange, targetTimeRange, {}));
  }
}

void ProcessScaledTimeRange(int64_t originFileDuration, int64_t fileDuration,
                            int64_t audioStartTime, int64_t audioDuration,
                            TimeRange& scaledTimeRange, std::vector<AudioClip>& audioClips,
                            std::shared_ptr<ByteData>& byteData) {
  auto audioEndTime = audioStartTime + audioDuration;
  if (audioEndTime <= scaledTimeRange.start) {
    // left outside
    ProcessLeftOutside(fileDuration, audioStartTime, audioDuration, audioClips, byteData);
  } else if (audioStartTime >= scaledTimeRange.end) {
    // right outside
    ProcessRightOutside(originFileDuration, fileDuration, audioStartTime, audioDuration,
                        scaledTimeRange, audioClips, byteData);
  } else if (audioStartTime <= scaledTimeRange.start && scaledTimeRange.end <= audioEndTime) {
    // contain
    ProcessScaledContainAudio(originFileDuration, fileDuration, audioStartTime, audioDuration,
                              scaledTimeRange, audioClips, byteData);
  } else if (scaledTimeRange.start <= audioStartTime && audioEndTime <= scaledTimeRange.end) {
    // contain
    ProcessAudioContainScaled(originFileDuration, fileDuration, audioStartTime, audioDuration,
                              scaledTimeRange, audioClips, byteData);
  } else if (audioStartTime < scaledTimeRange.start && scaledTimeRange.start <= audioEndTime) {
    // left intersect
    ProcessLeftIntersect(originFileDuration, fileDuration, audioStartTime, audioDuration,
                         scaledTimeRange, audioClips, byteData);
  } else {
    // right intersect
    ProcessRightIntersect(originFileDuration, fileDuration, audioStartTime, audioDuration,
                          scaledTimeRange, audioClips, byteData);
  }
}

bool AudioClip::ProcessPAGFileScale(PAGFile* file, std::vector<AudioClip>& audioClips,
                                    AudioAsset* media, std::shared_ptr<ByteData>& byteData) {
  if (file->_timeStretchMode != PAGTimeStretchMode::Scale) {
    return false;
  }
  auto originFileDuration = FrameToTime(file->file->duration(), file->file->frameRate());
  auto fileDuration = file->durationInternal();
  auto audioStartTime = file->audioStartTime();
  auto audioDuration = std::min(media->duration(), originFileDuration - audioStartTime);
  if (file->file->hasScaledTimeRange()) {
    auto scaledTimeRange = file->file->scaledTimeRange;
    scaledTimeRange.start = FrameToTime(scaledTimeRange.start, file->frameRateInternal());
    scaledTimeRange.end = FrameToTime(scaledTimeRange.end, file->frameRateInternal());
    ProcessScaledTimeRange(originFileDuration, fileDuration, audioStartTime, audioDuration,
                           scaledTimeRange, audioClips, byteData);
  } else {
    auto targetStart = audioStartTime * fileDuration / originFileDuration;
    auto targetEnd = targetStart + audioDuration * fileDuration / originFileDuration;
    auto sourceTimeRange = TimeRange{0, audioDuration};
    auto targetTimeRange = TimeRange{targetStart, targetEnd};
    audioClips.push_back(AudioClip(AudioSource(byteData), sourceTimeRange, targetTimeRange, {}));
  }
  return true;
}

void AudioClip::ProcessPAGAudioBytes(PAGComposition* composition,
                                     std::vector<AudioClip>& audioClips) {
  if (composition == nullptr || composition->audioBytes() == nullptr) {
    return;
  }
  auto data = new uint8_t[composition->audioBytes()->length()];
  memcpy(data, composition->audioBytes()->data(), composition->audioBytes()->length());
  auto byteData =
      std::shared_ptr<ByteData>(ByteData::MakeAdopted(data, composition->audioBytes()->length()));
  auto media = AudioAsset::Make(byteData);
  if (media == nullptr || media->tracks().empty()) {
    return;
  }
  if (composition->isPAGFile()) {
    auto file = static_cast<PAGFile*>(composition);
    if (ProcessPAGFileRepeat(file, audioClips, media.get(), byteData)) {
      return;
    }
    if (ProcessPAGFileScale(file, audioClips, media.get(), byteData)) {
      return;
    }
  }
  auto duration =
      std::min(media->duration(), composition->durationInternal() + composition->startTime() -
                                      composition->audioStartTime());
  auto sourceTimeRange = TimeRange{0, duration};
  auto targetTimeRange =
      TimeRange{composition->audioStartTime(), composition->audioStartTime() + duration};
  AudioClip clip(AudioSource(byteData), sourceTimeRange, targetTimeRange, {});
  audioClips.push_back(clip);
}

std::vector<AudioClip> AudioClip::ProcessImageLayer(PAGImageLayer* imageLayer) {
  auto movie = imageLayer->imageHolder->getMovie(imageLayer->editableIndex());
  if (movie == nullptr) {
    return {};
  }
  if (movie->layerOwner != imageLayer) {
    return {};
  }
  auto audioClips = movie->generateAudioClips();
  if (audioClips.empty()) {
    return audioClips;
  }
  auto imageFillRule = static_cast<ImageLayer*>(imageLayer->layer)->imageFillRule;
  if (imageFillRule == nullptr || imageFillRule->timeRemap == nullptr ||
      !imageFillRule->timeRemap->animatable()) {
    std::vector<AudioClip> result{};
    auto imageLayerDuration = imageLayer->contentDuration();
    for (auto& audioClip : audioClips) {
      if (audioClip.targetTimeRange.start >= imageLayerDuration) {
        continue;
      }
      if (audioClip.targetTimeRange.end > imageLayerDuration) {
        audioClip.sourceTimeRange.end = (imageLayerDuration - audioClip.targetTimeRange.start) *
                                            audioClip.sourceTimeRange.duration() /
                                            audioClip.targetTimeRange.duration() +
                                        audioClip.sourceTimeRange.start;
        audioClip.targetTimeRange.end = imageLayerDuration;
      }
      audioClip.rootFile = imageLayer->rootFile;
      result.push_back(audioClip);
    }
    return result;
  }
  return ProcessTimeRamp(imageLayer, audioClips,
                         static_cast<AnimatableProperty<Frame>*>(imageFillRule->timeRemap));
}

std::vector<AudioClip> AudioClip::ProcessTimeRamp(PAGImageLayer* imageLayer,
                                                  std::vector<AudioClip>& audioClips,
                                                  AnimatableProperty<Frame>* timeRemap) {
  std::vector<AudioClip> result{};
  auto imageLayerDuration = imageLayer->contentDuration();
  for (auto& keyframe : timeRemap->keyframes) {
    if (keyframe->startValue == keyframe->endValue || keyframe->startTime == keyframe->endTime) {
      // freeze frame
      continue;
    }
    auto sourceStartTime = FrameToTime(keyframe->startValue, imageLayer->frameRate());
    if (sourceStartTime >= imageLayerDuration) {
      continue;
    }
    auto sourceEndTime = FrameToTime(keyframe->endValue, imageLayer->frameRate());
    auto layerStartTime = imageLayer->getLayer()->startTime;
    auto targetStartTime =
        FrameToTime(keyframe->startTime - layerStartTime, imageLayer->frameRate());
    auto targetEndTime = FrameToTime(keyframe->endTime - layerStartTime, imageLayer->frameRate());
    if (sourceEndTime > imageLayerDuration) {
      targetEndTime = targetStartTime + (targetEndTime - targetStartTime) *
                                            (imageLayerDuration - sourceStartTime) /
                                            (sourceEndTime - sourceStartTime);
      sourceEndTime = imageLayerDuration;
    }
    auto source = TimeRange{sourceStartTime, sourceEndTime};
    auto target = TimeRange{targetStartTime, targetEndTime};
    for (auto audioClip : audioClips) {
      if (audioClip.applyTimeRamp(source, target)) {
        audioClip.rootFile = imageLayer->rootFile;
        result.push_back(audioClip);
      }
    }
  }
  return result;
}

std::vector<AudioClip> AudioClip::GenerateAudioClips(PAGImageLayer* fromImageLayer) {
  auto audioClips = ProcessImageLayer(fromImageLayer);
  if (!audioClips.empty()) {
    ShiftClipsWithLayer(audioClips, fromImageLayer);
  }
  return audioClips;
}

bool AudioClip::clearRootFile(PAGLayer* layer) {
  if (rootFile == layer) {
    rootFile = nullptr;
    return true;
  }
  return false;
}

void AudioClip::ShiftClipsWithLayer(std::vector<AudioClip>& audioClips, PAGLayer* layer) {
  auto startTime = layer->startTime();
  if (startTime == 0) {
    for (auto& audioClip : audioClips) {
      audioClip.clearRootFile(layer);
    }
    return;
  }
  if (startTime > 0) {
    for (auto& audioClip : audioClips) {
      ShiftClipsWithTime(audioClip, startTime);
      audioClip.clearRootFile(layer);
    }
    return;
  }
  auto time = -startTime;
  for (auto iter = audioClips.begin(); iter != audioClips.end(); ++iter) {
    auto& audioClip = *iter;
    if (audioClip.rootFile != nullptr) {
      if (!audioClip.clearRootFile(layer)) {
        continue;
      }
      ShiftClipsWithTime(audioClip, time);
    } else {
      if (audioClip.targetTimeRange.end <= time) {
        audioClips.erase(iter--);
      } else if (audioClip.targetTimeRange.start < time) {
        ClipWithTime(audioClip, time);
      } else {
        ShiftClipsWithTime(audioClip, -time);
      }
    }
  }
}

void AudioClip::ShiftClipsWithTime(AudioClip& audioClip, int64_t time) {
  audioClip.targetTimeRange.start += time;
  audioClip.targetTimeRange.end += time;
  for (auto& volumeRange : audioClip.volumeRanges) {
    volumeRange.timeRange.start += time;
    volumeRange.timeRange.end += time;
  }
}

void AudioClip::ClipWithTime(AudioClip& audioClip, int64_t time) {
  // 裁掉左边部分
  auto delta = time - audioClip.targetTimeRange.start;
  audioClip.sourceTimeRange.start +=
      audioClip.sourceTimeRange.duration() * delta / audioClip.targetTimeRange.duration();
  audioClip.targetTimeRange.start = time;
  audioClip.targetTimeRange.start -= time;
  audioClip.targetTimeRange.end -= time;
  for (auto volumeIter = audioClip.volumeRanges.begin(); volumeIter != audioClip.volumeRanges.end();
       ++volumeIter) {
    auto volumeRange = *volumeIter;
    if (volumeRange.timeRange.end <= time) {
      audioClip.volumeRanges.erase(volumeIter--);
    } else if (volumeRange.timeRange.start < time) {
      auto volumeDelta = time - volumeRange.timeRange.start;
      volumeRange.startVolume += (volumeRange.endVolume - volumeRange.startVolume) * volumeDelta /
                                 volumeRange.timeRange.duration();
      volumeRange.timeRange.start = time;
      volumeRange.timeRange.start -= time;
      volumeRange.timeRange.end -= time;
    } else {
      volumeRange.timeRange.start -= time;
      volumeRange.timeRange.end -= time;
    }
  }
}

std::vector<AudioClip> AudioClip::GenerateAudioClips(PAGComposition* fromComposition) {
  if (fromComposition == nullptr) {
    return {};
  }
  std::vector<AudioClip> audioClips{};
  ProcessPAGAudioBytes(fromComposition, audioClips);
  for (auto& childLayer : fromComposition->layers) {
    std::vector<AudioClip> clips;
    if (childLayer->layerType() == LayerType::PreCompose) {
      clips = GenerateAudioClips(static_cast<PAGComposition*>(childLayer.get()));
    } else if (childLayer->layerType() == LayerType::Image) {
      clips = GenerateAudioClips(static_cast<PAGImageLayer*>(childLayer.get()));
    } else {
      clips = {};
    }
    if (!clips.empty()) {
      ShiftClipsWithLayer(clips, fromComposition);
      audioClips.insert(audioClips.end(), clips.begin(), clips.end());
    }
  }
  return audioClips;
}

std::vector<int> AudioClip::ApplyAudioClips(AudioComposition* audioComposition,
                                            const std::vector<AudioClip>& audioClips) {
  if (audioClips.empty()) {
    return {};
  }
  std::vector<int> trackIDs{};
  for (auto& audioClip : audioClips) {
    auto audio = AudioAsset::Make(audioClip.audioSource);
    if (audio == nullptr || audio->tracks().empty()) {
      continue;
    }
    for (auto& track : audio->tracks()) {
      auto compositionTrack = audioComposition->addTrack();
      trackIDs.push_back(compositionTrack->trackID());
      compositionTrack->insertTimeRange(audioClip.sourceTimeRange, *track,
                                        audioClip.targetTimeRange.start);
      if (audioClip.targetTimeRange.duration() != audioClip.sourceTimeRange.duration()) {
        auto timeRange =
            TimeRange{audioClip.targetTimeRange.start,
                      audioClip.targetTimeRange.start + audioClip.sourceTimeRange.duration()};
        compositionTrack->scaleTimeRange(timeRange, audioClip.targetTimeRange.duration());
      }
      for (auto& volume : audioClip.volumeRanges) {
        compositionTrack->setVolumeRamp(volume.startVolume, volume.endVolume, volume.timeRange);
      }
    }
  }
  return trackIDs;
}

std::vector<int> AudioClip::DumpTracks(PAGComposition* fromComposition,
                                       AudioComposition* toAudioComposition) {
  auto audioClips = GenerateAudioClips(fromComposition);
  if (audioClips.empty()) {
    return {};
  }
  return ApplyAudioClips(toAudioComposition, audioClips);
}

bool AudioClip::applyTimeRamp(const TimeRange& source, const TimeRange& target) {
  if (targetTimeRange.end <= source.start || source.end <= targetTimeRange.start) {
    // outside
    return false;
  }
  if (targetTimeRange.start <= source.start && source.end <= targetTimeRange.end) {
    // inside
    // |--------|           audioClip.sourceTimeRange
    // |--------------|     audioClip.targetTimeRange
    //    |--------|        source
    //    |-----|           target
    AudioClip clip(*this);
    targetTimeRange = target;
    sourceTimeRange.start = (source.start - clip.targetTimeRange.start) *
                                clip.sourceTimeRange.duration() / clip.targetTimeRange.duration() +
                            clip.sourceTimeRange.start;
    sourceTimeRange.end = (source.end - clip.targetTimeRange.start) *
                              clip.sourceTimeRange.duration() / clip.targetTimeRange.duration() +
                          clip.sourceTimeRange.start;
  } else if (source.start <= targetTimeRange.start && targetTimeRange.end <= source.end) {
    // contain
    //    |-----|           audioClip.sourceTimeRange
    //    |--------|        audioClip.targetTimeRange
    // |--------------|     source
    // |-----|              target
    AudioClip clip(*this);
    targetTimeRange.start =
        (clip.targetTimeRange.start - source.start) * target.duration() / source.duration() +
        target.start;
    targetTimeRange.end =
        (clip.targetTimeRange.end - source.start) * target.duration() / source.duration() +
        target.start;
  } else if (source.start < targetTimeRange.start && targetTimeRange.start < source.end) {
    // left intersect
    //    |-----|       audioClip.sourceTimeRange
    //    |--------|    audioClip.targetTimeRange
    // |------|         source
    // |----|           target
    AudioClip clip(*this);
    targetTimeRange.end = target.end;
    targetTimeRange.start =
        (clip.targetTimeRange.start - source.start) * target.duration() / source.duration() +
        target.start;
    sourceTimeRange.end = (source.end - clip.targetTimeRange.start) *
                              clip.sourceTimeRange.duration() / clip.targetTimeRange.duration() +
                          clip.sourceTimeRange.start;
  } else {
    // right intersect
    // |-----|          audioClip.sourceTimeRange
    // |--------|       audioClip.targetTimeRange
    //      |------|    source
    //      |----|      target
    AudioClip clip(*this);
    targetTimeRange.start = target.start;
    targetTimeRange.end =
        (clip.targetTimeRange.end - source.start) * target.duration() / source.duration() +
        target.start;
    sourceTimeRange.start = (source.start - clip.targetTimeRange.start) *
                                clip.sourceTimeRange.duration() / clip.targetTimeRange.duration() +
                            clip.sourceTimeRange.start;
  }
  adjustVolumeRange();
  return true;
}

void AudioClip::adjustVolumeRange() {
  std::vector<VolumeRange> result{};
  for (auto& volumeRange : volumeRanges) {
    if (volumeRange.timeRange.end <= targetTimeRange.start ||
        targetTimeRange.end <= volumeRange.timeRange.start) {
      // outside
      continue;
    }
    if (volumeRange.timeRange.start <= targetTimeRange.start &&
        targetTimeRange.end <= volumeRange.timeRange.end) {
      // inside
      VolumeRange volume(volumeRange);
      volume.timeRange = targetTimeRange;
      volume.startVolume = static_cast<float>(targetTimeRange.start - volumeRange.timeRange.start) *
                               (volumeRange.endVolume - volumeRange.startVolume) /
                               volumeRange.timeRange.duration() +
                           volumeRange.startVolume;
      volume.endVolume = static_cast<float>(targetTimeRange.end - volumeRange.timeRange.start) *
                             (volumeRange.endVolume - volumeRange.startVolume) /
                             volumeRange.timeRange.duration() +
                         volumeRange.startVolume;
      result.push_back(volume);
      continue;
    }
    if (targetTimeRange.start <= volumeRange.timeRange.start &&
        volumeRange.timeRange.end <= targetTimeRange.end) {
      // contain
      result.push_back(volumeRange);
      continue;
    }
    // intersect
    VolumeRange volume(volumeRange);
    if (targetTimeRange.start < volumeRange.timeRange.start &&
        volumeRange.timeRange.start < targetTimeRange.end) {
      volume.timeRange.end = targetTimeRange.end;
      volume.endVolume = static_cast<float>(targetTimeRange.end - volume.timeRange.start) *
                             (volumeRange.endVolume - volumeRange.startVolume) /
                             volume.timeRange.duration() +
                         volumeRange.startVolume;
    } else {
      volume.timeRange.start = targetTimeRange.start;
      volume.startVolume = static_cast<float>(targetTimeRange.start - volume.timeRange.start) *
                               (volumeRange.endVolume - volumeRange.startVolume) /
                               volume.timeRange.duration() +
                           volumeRange.startVolume;
    }
    result.push_back(volume);
  }
  volumeRanges = result;
}
}  // namespace pag
#endif
