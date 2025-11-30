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
#include "audio/process/AudioTimeRange.h"
#include "base/utils/TimeUtil.h"
#include "rendering/editing/ImageReplacement.h"
#include "video/PAGMovie.h"

namespace pag {

static void TrimClipFromStart(AudioClip& clip, int64_t time) {
  auto delta = time - clip.targetTimeRange.start;
  clip.sourceTimeRange.start +=
      clip.sourceTimeRange.duration() * delta / clip.targetTimeRange.duration();
  clip.targetTimeRange.start = 0;
  clip.targetTimeRange.end -= time;
}

static void OffsetClipTime(AudioClip& clip, int64_t time) {
  clip.targetTimeRange.start += time;
  clip.targetTimeRange.end += time;
}

static void AdjustClipsByLayer(std::vector<AudioClip>& clips, std::shared_ptr<PAGLayer> layer) {
  auto startTime = layer->startTime();
  if (startTime == 0) {
    for (auto& clip : clips) {
      clip.clearRootFile(layer.get());
    }
    return;
  }
  if (startTime > 0) {
    for (auto& clip : clips) {
      OffsetClipTime(clip, startTime);
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
      OffsetClipTime(clip, time);
    } else {
      if (clip.targetTimeRange.end <= time) {
        clips.erase(iter--);
      } else if (clip.targetTimeRange.start < time) {
        TrimClipFromStart(clip, time);
      } else {
        OffsetClipTime(clip, -time);
      }
    }
  }
}

void AudioClip::ApplyClipsToAudio(std::shared_ptr<PAGComposition> composition,
                                  std::shared_ptr<AudioAsset> audio) {
  if (composition == nullptr) {
    return;
  }
  auto clips = GenerateClipsFromComposition(std::move(composition));
  ApplyClipsToAudio(std::move(audio), clips);
}

AudioClip::AudioClip(std::shared_ptr<AudioSource> audioSource, TimeRange sourceTimeRange,
                     TimeRange targetTimeRange)
    : sourceTimeRange(sourceTimeRange), targetTimeRange(targetTimeRange),
      source(std::move(audioSource)) {
}

bool AudioClip::operator==(const AudioClip& clip) const {
  return (*source == *clip.source && rootFile == clip.rootFile &&
          sourceTimeRange.start == clip.sourceTimeRange.start &&
          sourceTimeRange.end == clip.sourceTimeRange.end &&
          targetTimeRange.start == clip.targetTimeRange.start &&
          targetTimeRange.end == clip.targetTimeRange.end);
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
    AudioClip clip(*this);
    targetTimeRange = target;
    sourceTimeRange.start = (source.start - clip.targetTimeRange.start) *
                                clip.sourceTimeRange.duration() / clip.targetTimeRange.duration() +
                            clip.sourceTimeRange.start;
    sourceTimeRange.end = (source.end - clip.targetTimeRange.start) *
                              clip.sourceTimeRange.duration() / clip.targetTimeRange.duration() +
                          clip.sourceTimeRange.start;
  } else if (source.start <= targetTimeRange.start && targetTimeRange.end <= source.end) {
    AudioClip clip(*this);
    targetTimeRange.start =
        (clip.targetTimeRange.start - source.start) * target.duration() / source.duration() +
        target.start;
    targetTimeRange.end =
        (clip.targetTimeRange.end - source.start) * target.duration() / source.duration() +
        target.start;
  } else if (source.start < targetTimeRange.start && targetTimeRange.start < source.end) {
    AudioClip clip(*this);
    targetTimeRange.end = target.end;
    targetTimeRange.start =
        (clip.targetTimeRange.start - source.start) * target.duration() / source.duration() +
        target.start;
    sourceTimeRange.end = (source.end - clip.targetTimeRange.start) *
                              clip.sourceTimeRange.duration() / clip.targetTimeRange.duration() +
                          clip.sourceTimeRange.start;
  } else {
    AudioClip clip(*this);
    targetTimeRange.start = target.start;
    targetTimeRange.end =
        (clip.targetTimeRange.end - source.start) * target.duration() / source.duration() +
        target.start;
    sourceTimeRange.start = (source.start - clip.targetTimeRange.start) *
                                clip.sourceTimeRange.duration() / clip.targetTimeRange.duration() +
                            clip.sourceTimeRange.start;
  }
  return true;
}

bool AudioClip::ProcessRepeatedPAGFile(std::shared_ptr<PAGFile> file,
                                       std::shared_ptr<AudioAsset> audio,
                                       std::shared_ptr<AudioSource> source,
                                       std::vector<AudioClip>& clips) {
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
    auto clip = AudioClip(source, clipSourceTimeRange, clipTargetTimeRange);
    clips.push_back(clip);
    startTime += clipDuation;
  }
  return true;
}

bool AudioClip::ProcessScaledPAGFile(std::shared_ptr<PAGFile> file,
                                     std::shared_ptr<AudioAsset> audio,
                                     std::shared_ptr<AudioSource> source,
                                     std::vector<AudioClip>& clips) {
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
                           scaledTimeRange, clips, std::move(source));
  } else {
    auto targetStart = audioStartTime * duration / originalDuration;
    auto targetEnd = targetStart + audioDuration * duration / originalDuration;
    auto sourceTimeRange = TimeRange{0, audioDuration};
    auto targetTimeRange = TimeRange{targetStart, targetEnd};
    auto clip = AudioClip(std::move(source), sourceTimeRange, targetTimeRange);
    clips.push_back(clip);
  }
  return true;
}

void AudioClip::ProcessScaledTimeRange(int64_t originalDuration, int64_t duration,
                                       int64_t audioStartTime, int64_t audioDuration,
                                       TimeRange& scaledTimeRange, std::vector<AudioClip>& clips,
                                       std::shared_ptr<AudioSource> source) {
  auto audioEndTime = audioStartTime + audioDuration;
  if (audioEndTime <= scaledTimeRange.start) {
    ProcessAudioBeforeScaledRegion(duration, audioStartTime, audioDuration, clips,
                                   std::move(source));
  } else if (audioStartTime >= scaledTimeRange.end) {
    ProcessAudioAfterScaledRegion(originalDuration, duration, audioStartTime, audioDuration,
                                  scaledTimeRange, clips, std::move(source));
  } else if (audioStartTime <= scaledTimeRange.start && scaledTimeRange.end <= audioEndTime) {
    ProcessAudioWrapsScaledRegion(originalDuration, duration, audioStartTime, audioDuration,
                                  scaledTimeRange, clips, std::move(source));
  } else if (scaledTimeRange.start <= audioStartTime && audioEndTime <= scaledTimeRange.end) {
    ProcessAudioInsideScaledRegion(originalDuration, duration, audioStartTime, audioDuration,
                                   scaledTimeRange, clips, std::move(source));
  } else if (audioStartTime < scaledTimeRange.start && scaledTimeRange.start <= audioEndTime) {
    ProcessAudioOverlapsScaledStart(originalDuration, duration, audioStartTime, audioDuration,
                                    scaledTimeRange, clips, std::move(source));
  } else {
    ProcessAudioOverlapsScaledEnd(originalDuration, duration, audioStartTime, audioDuration,
                                  scaledTimeRange, clips, std::move(source));
  }
}

std::vector<AudioClip> AudioClip::GenerateClipsFromComposition(
    std::shared_ptr<PAGComposition> composition) {
  if (composition == nullptr) {
    return {};
  }
  std::vector<AudioClip> clips = GenerateClipsFromAudioBytes(composition);
  for (auto& layer : composition->layers) {
    std::vector<AudioClip> layerClips = {};
    if (layer->layerType() == LayerType::PreCompose) {
      layerClips = GenerateClipsFromComposition(std::static_pointer_cast<PAGComposition>(layer));
    } else if (layer->layerType() == LayerType::Image) {
      layerClips = GenerateClipsFromImageLayer(std::static_pointer_cast<PAGImageLayer>(layer));
    }
    if (layerClips.empty()) {
      continue;
    }
    AdjustClipsByLayer(layerClips, composition);
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

std::vector<AudioClip> AudioClip::GenerateClipsFromAudioBytes(
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
    if (ProcessRepeatedPAGFile(file, audio, source, clips)) {
      return clips;
    }
    if (ProcessScaledPAGFile(file, audio, source, clips)) {
      return clips;
    }
  }
  auto duration = std::min(audio->duration(), composition->duration() + composition->startTime() -
                                                  composition->audioStartTime());
  auto sourceTimeRange = TimeRange{0, duration};
  auto targetTimeRange = TimeRange{composition->startTime(), composition->startTime() + duration};
  auto clip = AudioClip(source, sourceTimeRange, targetTimeRange);
  clips.push_back(clip);
  return clips;
}

std::vector<AudioClip> AudioClip::GenerateClipsFromImageLayer(
    std::shared_ptr<PAGImageLayer> layer) {
  auto clips = ExtractClipsFromImageLayer(layer);
  if (!clips.empty()) {
    AdjustClipsByLayer(clips, std::move(layer));
  }
  return clips;
}

std::vector<AudioClip> AudioClip::ExtractClipsFromImageLayer(std::shared_ptr<PAGImageLayer> layer) {
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
  return ApplyTimeRemapToClips(std::move(layer), audioClips,
                               static_cast<AnimatableProperty<Frame>*>(imageFillRule->timeRemap));
}

std::vector<AudioClip> AudioClip::ApplyTimeRemapToClips(std::shared_ptr<PAGImageLayer> layer,
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

void AudioClip::ApplyClipsToAudio(std::shared_ptr<AudioAsset> audio,
                                  const std::vector<AudioClip>& clips) {
  if (clips.empty()) {
    return;
  }
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
    }
  }
}

}  // namespace pag
