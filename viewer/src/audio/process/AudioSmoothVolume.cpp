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

#include "AudioSmoothVolume.h"

namespace pag {

bool IsFloatEqual(float a, float b) {
  return std::fabs(a - b) < 1e-6;
}

bool IsGainValid(float gain) {
  return gain >= 0 && !IsFloatEqual(gain, 1.0f);
}

std::shared_ptr<AudioSmoothVolume> AudioSmoothVolume::Make(
    std::shared_ptr<AudioTrack> track, std::shared_ptr<AudioOutputConfig> outputConfig) {
  if (track->volumeRangeList.empty()) {
    return nullptr;
  }
  auto smoothVolume = new AudioSmoothVolume(std::move(track), std::move(outputConfig));
  return std::shared_ptr<AudioSmoothVolume>(smoothVolume);
}

void AudioSmoothVolume::process(int64_t time, std::shared_ptr<ByteData> data) {
  if (data == nullptr || data->length() <= 0 || time < 0) {
    return;
  }
  float currentGain = 1.0f;
  float targetGain = 1.0f;
  calcSmoothVolumeRange(time, currentGain, targetGain);
  smoothVolumeProcessing(data->data(), static_cast<int>(data->length()), outputConfig->channels,
                         currentGain, targetGain);
}

void AudioSmoothVolume::seek(int64_t time) {
  currentVolumeIndex = 0;
  currentVolume = 1.0f;
  updateGainGap = true;
  volumeGapInFrame = 0.0f;
  if (time < 0) {
    return;
  }
  for (auto& volumeControl : volumeControlInfos) {
    if (volumeControl.endTime < time) {
      currentVolumeIndex++;
      currentVolume = volumeControl.targetVolume;
    } else if (volumeControl.startTime <= time) {
      currentVolume = (volumeControl.targetVolume - volumeControl.startVolume) *
                          static_cast<float>(time - volumeControl.startTime) /
                          static_cast<float>(volumeControl.endTime - volumeControl.startTime) +
                      volumeControl.startVolume;
      volumeGapInFrame = (volumeControl.targetVolume - volumeControl.startVolume) /
                         static_cast<float>(volumeControl.totalFrameNumber);
      updateGainGap = false;
    }
  }
}

void AudioSmoothVolume::smoothVolumeProcessing(uint8_t* audioBuffer, int byteSize,
                                               int channelNumber, float currentGain,
                                               float targetGain) {
  if (IsFloatEqual(currentGain, targetGain)) {
    if (currentGain == 1.0) {
      return;
    }
    gainApplyProcess(currentGain, audioBuffer, byteSize);
  } else {
    int16_t nSampleNum = byteSize / 2;
    int16_t nSamplePerChannel = nSampleNum / channelNumber;
    auto fGainGap =
        static_cast<double>((targetGain - currentGain) / static_cast<float>(nSamplePerChannel));
    auto pAudioBuf = reinterpret_cast<int16_t*>(audioBuffer);
    int32_t nResult;
    if (IsFloatEqual(fGainGap, 0.0f)) {
      gainApplyProcess(targetGain, audioBuffer, byteSize);
    } else {
      for (int ich = 0; ich < channelNumber; ich++) {
        for (int idx = 0; idx < nSamplePerChannel; idx++) {
          nResult = static_cast<int32_t>(pAudioBuf[idx * channelNumber + ich] *
                                         (currentGain + (idx + 1) * fGainGap));
          if (nResult > SHRT_MAX) {
            nResult = SHRT_MAX;
          } else if (nResult < SHRT_MIN) {
            nResult = SHRT_MIN;
          }
          pAudioBuf[idx * channelNumber + ich] = static_cast<int16_t>(nResult);
        }
      }
    }
  }
}

void AudioSmoothVolume::gainApplyProcess(float gain, uint8_t* buffer, int byteSize) {
  if (!IsGainValid(gain)) {
    return;
  }

  int16_t oldValue = 0;
  int16_t newValue = 0;
  int32_t mulValue = 0;
  int32_t sampleNum = byteSize / 2;
  auto pBuf = reinterpret_cast<int16_t*>(buffer);
  float rounding = 0.0f;
  for (int index = 0; index < sampleNum; index += 2) {
    oldValue = pBuf[index];
    rounding = (oldValue >= 0) ? 0.5 : -0.5f;
    mulValue = static_cast<int32_t>(static_cast<float>(oldValue) * gain + rounding);
    if (mulValue > SHRT_MAX) {
      newValue = SHRT_MAX;
    } else if (mulValue < SHRT_MIN) {
      newValue = SHRT_MIN;
    } else {
      newValue = static_cast<int16_t>(mulValue);
    }
    pBuf[index] = newValue;

    oldValue = pBuf[index + 1];
    rounding = (oldValue >= 0) ? 0.5 : -0.5f;
    mulValue = static_cast<int32_t>(static_cast<float>(oldValue) * gain + rounding);
    if (mulValue > SHRT_MAX) {
      newValue = SHRT_MAX;
    } else if (mulValue < SHRT_MIN) {
      newValue = SHRT_MIN;
    } else {
      newValue = static_cast<int16_t>(mulValue);
    }
    pBuf[index + 1] = newValue;
  }
}

void AudioSmoothVolume::calcSmoothVolumeRange(int64_t inputPts, float& currentGain,
                                              float& targetGain) {
  if (inputPts < volumeControlInfos[currentVolumeIndex].startTime ||
      inputPts > volumeControlInfos[volumeControlInfos.size() - 1].endTime) {
    currentGain = currentVolume;
    targetGain = currentVolume;
    return;
  }

  bool isGetActualVolumeRange = false;
  if (inputPts > volumeControlInfos[currentVolumeIndex].endTime) {
    currentVolumeIndex++;
    if (inputPts >= volumeControlInfos[currentVolumeIndex].startTime) {
      isGetActualVolumeRange = true;
      updateGainGap = true;
    } else {
      isGetActualVolumeRange = false;
    }
  } else if (inputPts >= volumeControlInfos[currentVolumeIndex].startTime) {
    isGetActualVolumeRange = true;
  }

  if (isGetActualVolumeRange) {
    if (!updateGainGap &&
        IsFloatEqual(volumeControlInfos[currentVolumeIndex].targetVolume, currentVolume)) {
      currentGain = currentVolume;
      targetGain = currentVolume;
      updateGainGap = true;
    } else {
      if (updateGainGap) {
        auto& volumeControl = volumeControlInfos[currentVolumeIndex];
        currentVolume = volumeControl.startVolume;
        volumeGapInFrame = (volumeControl.targetVolume - currentVolume) /
                           static_cast<float>(volumeControl.totalFrameNumber);
        updateGainGap = false;
      }
      currentGain = currentVolume;
      targetGain = std::max(currentVolume + volumeGapInFrame, 0.0f);
      currentVolume = targetGain;
    }
  } else {
    currentGain = currentVolume;
    targetGain = currentVolume;
    updateGainGap = true;
  }
}

AudioSmoothVolume::AudioSmoothVolume(std::shared_ptr<AudioTrack> track,
                                     std::shared_ptr<AudioOutputConfig> outputConfig)
    : track(std::move(track)), outputConfig(outputConfig) {
  auto frameDuration = outputConfig->outputSamplesCount * 1000000.0 / outputConfig->sampleRate;
  for (auto& iter : this->track->volumeRangeList) {
    auto volumeRange = iter;
    VolumeControlInfo volumeInfo;
    volumeInfo.startTime = volumeRange.timeRange.start;
    volumeInfo.endTime = volumeRange.timeRange.end;
    volumeInfo.startVolume = std::max(volumeRange.startVolume, 0.0f);
    volumeInfo.targetVolume = std::max(volumeRange.endVolume, 0.0f);
    volumeInfo.totalFrameNumber = static_cast<int>(
        static_cast<double>(volumeInfo.endTime - volumeInfo.startTime) / frameDuration);
    volumeInfo.totalFrameNumber =
        (volumeInfo.totalFrameNumber <= 0) ? 1 : volumeInfo.totalFrameNumber;
    if (IsFloatEqual(volumeRange.startVolume, volumeRange.endVolume)) {
      volumeInfo.totalFrameNumber = 1;
    }
    volumeControlInfos.push_back(volumeInfo);
  }
}

}  // namespace pag