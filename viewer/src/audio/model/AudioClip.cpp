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

#include "AudioClip.h"
#include <memory>
#include <utility>
#include "base/utils/TimeUtil.h"
#include "rendering/editing/ImageReplacement.h"
#include "video/PAGMovie.h"

namespace pag {

void ProcessLeftOutside(int64_t fileDuration, int64_t audioStartTime, int64_t audioDuration,
                        std::vector<AudioClip>& clips, std::shared_ptr<AudioSource> source,
                        float volume) {
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
    int64_t duration = std::min(audioDuration, fileDuration - audioStartTime);
    if (duration > 0) {
      auto sourceTimeRange = TimeRange{0, duration};
      auto targetTimeRange = TimeRange{audioStartTime, audioStartTime + duration};
      auto clip =
          AudioClip(source, sourceTimeRange, targetTimeRange, {{targetTimeRange, volume, volume}});
      clips.push_back(clip);
    }
  }
}

void ProcessRightOutside(int64_t originFileDuration, int64_t fileDuration, int64_t audioStartTime,
                         int64_t audioDuration, TimeRange& scaledTimeRange,
                         std::vector<AudioClip>& clips, std::shared_ptr<AudioSource> source,
                         float volume) {
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
    auto clip = AudioClip(std::move(source), sourceTimeRange, targetTimeRange,
                          {{targetTimeRange, volume, volume}});
    clips.push_back(clip);
  }
}

void ProcessScaledContainAudio(int64_t originFileDuration, int64_t fileDuration,
                               int64_t audioStartTime, int64_t audioDuration,
                               TimeRange& scaledTimeRange, std::vector<AudioClip>& clips,
                               std::shared_ptr<AudioSource> source, float volume) {
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
    auto clip =
        AudioClip(source, sourceTimeRange, targetTimeRange, {{targetTimeRange, volume, volume}});
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
    auto clip =
        AudioClip(source, sourceTimeRange, targetTimeRange, {{targetTimeRange, volume, volume}});
    clips.push_back(clip);
  }
  auto targetStart = scaledTimeRange.end + std::max(fileDuration, minDuration) - originFileDuration;
  if (fileDuration > targetStart) {
    auto duration = std::min(audioEndTime - scaledTimeRange.end, fileDuration - targetStart);
    auto sourceStart = scaledTimeRange.end - audioStartTime;
    auto sourceTimeRange = TimeRange{sourceStart, sourceStart + duration};
    auto targetTimeRange = TimeRange{targetStart, targetStart + duration};
    auto clip =
        AudioClip(source, sourceTimeRange, targetTimeRange, {{targetTimeRange, volume, volume}});
    clips.push_back(clip);
  }
}

void ProcessAudioContainScaled(int64_t originFileDuration, int64_t fileDuration,
                               int64_t audioStartTime, int64_t audioDuration,
                               TimeRange& scaledTimeRange, std::vector<AudioClip>& clips,
                               std::shared_ptr<AudioSource> source, float volume) {
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
    auto clip = AudioClip(std::move(source), sourceTimeRange, targetTimeRange,
                          {{targetTimeRange, volume, volume}});
    clips.push_back(clip);
  }
}

void ProcessLeftIntersect(int64_t originFileDuration, int64_t fileDuration, int64_t audioStartTime,
                          int64_t audioDuration, TimeRange& scaledTimeRange,
                          std::vector<AudioClip>& clips, std::shared_ptr<AudioSource> source,
                          float volume) {
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
    auto targetTimeRange = TimeRange{audioStartTime, audioStartTime + duration};
    auto clip = AudioClip(source, TimeRange{0, duration}, targetTimeRange,
                          {{targetTimeRange, volume, volume}});
    clips.push_back(clip);
    auto minDuration = originFileDuration - scaledTimeRange.duration();
    if (fileDuration > minDuration) {
      auto targetStart = scaledTimeRange.start;
      auto targetEnd = targetStart + (audioEndTime - scaledTimeRange.start) *
                                         (fileDuration - minDuration) / scaledTimeRange.duration();
      auto sourceTimeRange = TimeRange{duration, audioDuration};
      targetTimeRange = TimeRange{targetStart, targetEnd};
      clip =
          AudioClip(source, sourceTimeRange, targetTimeRange, {{targetTimeRange, volume, volume}});
      clips.push_back(clip);
    }
  }
}

void ProcessRightIntersect(int64_t originFileDuration, int64_t fileDuration, int64_t audioStartTime,
                           int64_t audioDuration, TimeRange& scaledTimeRange,
                           std::vector<AudioClip>& clips, std::shared_ptr<AudioSource> source,
                           float volume) {
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
      auto clip =
          AudioClip(source, sourceTimeRange, targetTimeRange, {{targetTimeRange, volume, volume}});
      clips.push_back(clip);
    }
    auto audioTargetEnd = audioEndTime + offset;
    auto sourceTimeRange = TimeRange{scaledTimeRange.end - audioStartTime, audioDuration};
    auto targetTimeRange = TimeRange{scaledTargetEnd, audioTargetEnd};
    auto clip =
        AudioClip(source, sourceTimeRange, targetTimeRange, {{targetTimeRange, volume, volume}});
    clips.push_back(clip);
  }
}

std::vector<int> AudioClip::DumpTracks(std::shared_ptr<PAGComposition> composition,
                                       std::shared_ptr<AudioAsset> audio, float volume) {
  if (composition == nullptr) {
    return {};
  }
  auto clips = GenerateAudioClips(std::move(composition));
  return ApplyAudioClips(std::move(audio), clips, volume);
}

std::vector<AudioClip> AudioClip::GenerateAudioClips(std::shared_ptr<PAGComposition> composition) {
  if (composition == nullptr) {
    return {};
  }
  std::vector<AudioClip> clips = GenerateAudioClipsFromAudioBytes(composition);
  for (auto& layer : composition->layers) {
    std::vector<AudioClip> layerClips = {};
    if (layer->layerType() == LayerType::PreCompose) {
      layerClips = GenerateAudioClips(std::static_pointer_cast<PAGComposition>(layer));
    } else if (layer->layerType() == LayerType::Image) {
      layerClips = GenerateAudioClipsFromImageLayer(std::static_pointer_cast<PAGImageLayer>(layer));
    }
    if (layerClips.empty()) {
      continue;
    }
    ShiftClipsWithLayer(layerClips, composition);
    for (const auto& layerClip : layerClips) {
      auto iter = std::find_if(clips.begin(), clips.end(),
                               [&](const AudioClip& clip) { return clip == layerClip; });
      if (iter == clips.end()) {
        clips.push_back(layerClip);
      }
    }
  }
  return clips;
}

AudioClip::AudioClip(std::shared_ptr<AudioSource> audioSource, TimeRange sourceTimeRange,
                     TimeRange targetTimeRange, const std::vector<VolumeRange>& volumeRanges)
    : sourceTimeRange(sourceTimeRange), targetTimeRange(targetTimeRange),
      volumeRanges(volumeRanges), source(std::move(audioSource)) {
}

bool AudioClip::operator==(const AudioClip& clip) const {
  return (*source == *clip.source && rootFile == clip.rootFile &&
          sourceTimeRange.start == clip.sourceTimeRange.start &&
          sourceTimeRange.end == clip.sourceTimeRange.end &&
          targetTimeRange.start == clip.targetTimeRange.start &&
          targetTimeRange.end == clip.targetTimeRange.end);
}

std::vector<AudioClip> AudioClip::GenerateAudioClipsFromAudioBytes(
    std::shared_ptr<PAGComposition> composition) {
  if (composition == nullptr || composition->audioBytes() == nullptr) {
    return {};
  }
  auto uniqueByteData =
      ByteData::MakeCopy(composition->audioBytes()->data(), composition->audioBytes()->length());
  auto byteData = std::shared_ptr<ByteData>(std::move(uniqueByteData));
  auto audio = AudioAsset::Make(byteData);
  auto source = audio->getSource();
  if (audio == nullptr || audio->getTracks().empty()) {
    return {};
  }

  std::vector<AudioClip> clips = {};
  if (composition->isPAGFile()) {
    auto file = std::static_pointer_cast<PAGFile>(composition);
    if (file == nullptr) {
      return {};
    }
    if (ProcessRepeatedPAGFile(file, audio, source, clips, 1.0f)) {
      return clips;
    }
    if (ProcessScaledPAGFile(file, audio, source, clips, 1.0f)) {
      return clips;
    }
  }
  auto duration = std::min(audio->duration(), composition->duration() + composition->startTime() -
                                                  composition->audioStartTime());
  auto sourceTimeRange = TimeRange{0, duration};
  auto targetTimeRange = TimeRange{composition->startTime(), composition->startTime() + duration};
  auto clip = AudioClip(source, sourceTimeRange, targetTimeRange, {});
  clips.push_back(clip);
  return clips;
}

bool AudioClip::ProcessRepeatedPAGFile(std::shared_ptr<PAGFile> file,
                                       std::shared_ptr<AudioAsset> audio,
                                       std::shared_ptr<AudioSource> source,
                                       std::vector<AudioClip>& clips, float volume) {
  if (file->timeStretchMode() == PAGTimeStretchMode::Repeat) {
    return false;
  }
  auto originalDuration = FrameToTime(file->file->duration(), file->file->frameRate());
  auto duration = file->duration();
  if (duration <= originalDuration) {
    return false;
  }
  auto audioStartTime = file->audioStartTime();
  auto originalAudioDuration = originalDuration - audioStartTime;
  auto startTime = audioStartTime;
  while (duration > startTime) {
    auto clipDuation = std::min(duration - startTime, originalAudioDuration);
    clipDuation = std::min(clipDuation, audio->duration());
    auto clipSourceTimeRange = TimeRange{0, clipDuation};
    auto clipTargetTimeRange = TimeRange{startTime, startTime + clipDuation};
    auto clip = AudioClip(source, clipSourceTimeRange, clipTargetTimeRange,
                          {{clipTargetTimeRange, volume, volume}});
    clips.push_back(clip);
    startTime += clipDuation;
  }
  return true;
}

bool AudioClip::ProcessScaledPAGFile(std::shared_ptr<PAGFile> file,
                                     std::shared_ptr<AudioAsset> audio,
                                     std::shared_ptr<AudioSource> source,
                                     std::vector<AudioClip>& clips, float volume) {
  if (file->timeStretchMode() == PAGTimeStretchMode::Scale) {
    return false;
  }
  auto originalDuration = FrameToTime(file->file->duration(), file->file->frameRate());
  auto duration = file->duration();
  auto audioStartTime = file->audioStartTime();
  auto audioDuration = std::min(audio->duration(), originalDuration - audioStartTime);
  if (file->file->hasScaledTimeRange()) {
    auto scaledTimeRange = file->file->scaledTimeRange;
    scaledTimeRange.start = FrameToTime(scaledTimeRange.start, file->frameRate());
    scaledTimeRange.end = FrameToTime(scaledTimeRange.end, file->frameRate());
    ProcessScaledTimeRange(originalDuration, duration, audioStartTime, audioDuration,
                           scaledTimeRange, clips, std::move(source), volume);
  } else {
    auto targetStart = audioStartTime * duration / originalDuration;
    auto targetEnd = targetStart + audioDuration * duration / originalDuration;
    auto sourceTimeRange = TimeRange{0, audioDuration};
    auto targetTimeRange = TimeRange{targetStart, targetEnd};
    auto clip = AudioClip(std::move(source), sourceTimeRange, targetTimeRange,
                          {{targetTimeRange, volume, volume}});
    clips.push_back(clip);
  }
  return true;
}

void AudioClip::ProcessScaledTimeRange(int64_t originalDuration, int64_t duration,
                                       int64_t audioStartTime, int64_t audioDuration,
                                       TimeRange& scaledTimeRange, std::vector<AudioClip>& clips,
                                       std::shared_ptr<AudioSource> source, float volume) {
  auto audioEndTime = audioStartTime + audioDuration;
  if (audioEndTime <= scaledTimeRange.start) {
    ProcessLeftOutside(duration, audioStartTime, audioDuration, clips, std::move(source), volume);
  } else if (audioStartTime >= scaledTimeRange.end) {
    ProcessRightOutside(originalDuration, duration, audioStartTime, audioDuration, scaledTimeRange,
                        clips, std::move(source), volume);
  } else if (audioStartTime <= scaledTimeRange.start && scaledTimeRange.end <= audioEndTime) {
    ProcessScaledContainAudio(originalDuration, duration, audioStartTime, audioDuration,
                              scaledTimeRange, clips, std::move(source), volume);
  } else if (scaledTimeRange.start <= audioStartTime && audioEndTime <= scaledTimeRange.end) {
    ProcessAudioContainScaled(originalDuration, duration, audioStartTime, audioDuration,
                              scaledTimeRange, clips, std::move(source), volume);
  } else if (audioStartTime < scaledTimeRange.start && scaledTimeRange.start <= audioEndTime) {
    ProcessLeftIntersect(originalDuration, duration, audioStartTime, audioDuration, scaledTimeRange,
                         clips, std::move(source), volume);
  } else {
    ProcessRightIntersect(originalDuration, duration, audioStartTime, audioDuration,
                          scaledTimeRange, clips, std::move(source), volume);
  }
}

std::vector<AudioClip> AudioClip::ProcessImageLayer(std::shared_ptr<PAGImageLayer> layer) {
  auto replacement = layer->replacement;
  if (replacement == nullptr) {
    return {};
  }
  auto pagImage = replacement->getImage();
  if (pagImage == nullptr || pagImage->isStill()) {
    return {};
  }
  auto movie = std::static_pointer_cast<PAGMovie>(pagImage);
  auto audioClips = movie->generateAudioClips();
  if (audioClips.empty()) {
    return audioClips;
  }
  auto imageFillRule = static_cast<ImageLayer*>(layer->layer)->imageFillRule;
  if (imageFillRule == nullptr || imageFillRule->timeRemap == nullptr ||
      !imageFillRule->timeRemap->animatable()) {
    std::vector<AudioClip> result{};
    auto imageLayerDuration = layer->contentDurationInternal();
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
      audioClip.rootFile = layer->rootFile;
      result.push_back(audioClip);
    }
    return result;
  }
  return ProcessTimeRamp(std::move(layer), audioClips,
                         static_cast<AnimatableProperty<Frame>*>(imageFillRule->timeRemap));
}

std::vector<AudioClip> AudioClip::ProcessTimeRamp(std::shared_ptr<PAGImageLayer> layer,
                                                  std::vector<AudioClip>& clips,
                                                  AnimatableProperty<Frame>* timeRemap) {
  std::vector<AudioClip> result = {};
  auto imageLayerDuration = layer->contentDurationInternal();
  for (auto& keyframe : timeRemap->keyframes) {
    if (keyframe->startValue == keyframe->endValue || keyframe->startTime == keyframe->endTime) {
      continue;
    }
    auto sourceStartTime = FrameToTime(keyframe->startValue, layer->frameRate());
    if (sourceStartTime >= imageLayerDuration) {
      continue;
    }
    auto sourceEndTime = FrameToTime(keyframe->endValue, layer->frameRate());
    auto layerStartTime = layer->getLayer()->startTime;
    auto targetStartTime = FrameToTime(keyframe->startTime - layerStartTime, layer->frameRate());
    auto targetEndTime = FrameToTime(keyframe->endTime - layerStartTime, layer->frameRate());
    if (sourceEndTime > imageLayerDuration) {
      targetEndTime = targetStartTime + (targetEndTime - targetStartTime) *
                                            (imageLayerDuration - sourceStartTime) /
                                            (sourceEndTime - sourceStartTime);
      sourceEndTime = imageLayerDuration;
    }
    auto source = TimeRange{sourceStartTime, sourceEndTime};
    auto target = TimeRange{targetStartTime, targetEndTime};
    for (auto audioClip : clips) {
      if (audioClip.applyTimeRamp(source, target)) {
        audioClip.rootFile = layer->rootFile;
        result.push_back(audioClip);
      }
    }
  }
  return result;
}

std::vector<AudioClip> AudioClip::GenerateAudioClipsFromImageLayer(
    std::shared_ptr<PAGImageLayer> layer) {
  auto clips = ProcessImageLayer(layer);
  if (!clips.empty()) {
    ShiftClipsWithLayer(clips, std::move(layer));
  }
  return clips;
}

void AudioClip::ShiftClipsWithLayer(std::vector<AudioClip>& clips,
                                    std::shared_ptr<PAGLayer> layer) {
  auto startTime = layer->startTime();
  if (startTime == 0) {
    for (auto& clip : clips) {
      clip.clearRootFile(layer.get());
    }
    return;
  }
  if (startTime > 0) {
    for (auto& clip : clips) {
      ShiftClipWithTime(clip, startTime);
      clip.clearRootFile(layer.get());
    }
    return;
  }
  auto time = startTime;
  for (auto iter = clips.begin(); iter != clips.end(); ++iter) {
    auto& clip = *iter;
    if (clip.rootFile != nullptr) {
      if (!clip.clearRootFile(layer.get())) {
        continue;
      }
      ShiftClipWithTime(clip, time);
    } else {
      if (clip.targetTimeRange.end <= time) {
        clips.erase(iter--);
      } else if (clip.targetTimeRange.start < time) {
        ClipWithTime(clip, time);
      } else {
        ShiftClipWithTime(clip, -time);
      }
    }
  }
}

void AudioClip::ClipWithTime(AudioClip& clip, int64_t time) {
  auto delta = time - clip.targetTimeRange.start;
  clip.sourceTimeRange.start +=
      clip.sourceTimeRange.duration() * delta / clip.targetTimeRange.duration();
  clip.targetTimeRange.start = time;
  clip.targetTimeRange.start -= time;
  clip.targetTimeRange.end -= time;
  for (auto volumeIter = clip.volumeRanges.begin(); volumeIter != clip.volumeRanges.end();
       ++volumeIter) {
    auto volumeRange = *volumeIter;
    if (volumeRange.timeRange.end <= time) {
      clip.volumeRanges.erase(volumeIter--);
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

void AudioClip::ShiftClipWithTime(AudioClip& clip, int64_t time) {
  clip.targetTimeRange.start += time;
  clip.targetTimeRange.end += time;
  for (auto& volumeRange : clip.volumeRanges) {
    volumeRange.timeRange.start += time;
    volumeRange.timeRange.end += time;
  }
}

std::vector<int> AudioClip::ApplyAudioClips(std::shared_ptr<AudioAsset> audio,
                                            const std::vector<AudioClip>& clips, float volume) {
  if (clips.empty()) {
    return {};
  }
  std::vector<int> trackIDs = {};
  for (auto& clip : clips) {
    auto theAudio = AudioAsset::Make(clip.source);
    if (theAudio == nullptr || theAudio->getTracks().empty()) {
      continue;
    }
    for (auto& track : theAudio->getTracks()) {
      auto newTrack = audio->addTrack();
      newTrack->insetTimeRange(clip.sourceTimeRange, *track, clip.targetTimeRange.start);
      if (clip.targetTimeRange.duration() != clip.sourceTimeRange.duration()) {
        auto timeRange = TimeRange{clip.targetTimeRange.start,
                                   clip.targetTimeRange.start + clip.sourceTimeRange.duration()};
        newTrack->scaleTimeRange(timeRange, clip.targetTimeRange.duration());
      }
      for (auto& volumeRange : clip.volumeRanges) {
        newTrack->addVolumeRange(volumeRange.startVolume * volume, volumeRange.endVolume * volume,
                                 volumeRange.timeRange);
      }
      if (clip.volumeRanges.empty()) {
        newTrack->addVolumeRange(volume, volume, clip.targetTimeRange);
      }
      trackIDs.push_back(newTrack->getTrackID());
    }
  }
  return trackIDs;
}

void AudioClip::adjustVolumeRange() {
  std::vector<VolumeRange> result{};
  for (auto& volumeRange : volumeRanges) {
    if (volumeRange.timeRange.end <= targetTimeRange.start ||
        targetTimeRange.end <= volumeRange.timeRange.start) {
      continue;
    }
    if (volumeRange.timeRange.start <= targetTimeRange.start &&
        targetTimeRange.end <= volumeRange.timeRange.end) {
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
      result.push_back(volumeRange);
      continue;
    }
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

bool AudioClip::clearRootFile(PAGLayer* layer) {
  if (rootFile == layer) {
    rootFile = nullptr;
    return true;
  }
  return false;
}

bool AudioClip::applyTimeRamp(const TimeRange& source, const TimeRange& target) {
  if (targetTimeRange.end <= source.start || source.end <= targetTimeRange.start) {
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

}  // namespace pag
