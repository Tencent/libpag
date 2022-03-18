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

#include "AudioSmoothVolume.h"

#define FLOAT_EQUAL(a, b) (fabs(a - b) < 1e-6 ? true : false)

namespace pag {
std::unique_ptr<AudioSmoothVolume> AudioSmoothVolume::Make(
    const std::shared_ptr<AudioTrack>& track, std::shared_ptr<PCMOutputConfig> pcmOutputConfig) {
  if (track->type() != "AudioCompositionTrack") {
    return nullptr;
  }
  auto compTrack = std::static_pointer_cast<AudioCompositionTrack>(track);
  if (compTrack->volumeRampList.empty()) {
    return nullptr;
  }
  return std::unique_ptr<AudioSmoothVolume>(
      new AudioSmoothVolume(compTrack, std::move(pcmOutputConfig)));
}

AudioSmoothVolume::AudioSmoothVolume(std::shared_ptr<AudioCompositionTrack> track,
                                     std::shared_ptr<PCMOutputConfig> pcmOutputConfig)
    : track(std::move(track)), pcmOutputConfig(std::move(pcmOutputConfig)) {
  auto frameDuration =
      this->pcmOutputConfig->outputSamplesCount * 1000000.0 / this->pcmOutputConfig->sampleRate;
  for (auto& iter : this->track->volumeRampList) {
    auto volumeProInfo = iter;
    auto volumeRamp = std::get<1>(iter);
    VolumeControlInfo volumeInfo;
    volumeInfo.startTime = std::get<0>(volumeProInfo).start;
    volumeInfo.endTime = std::get<0>(volumeProInfo).end;
    volumeInfo.startVolume = std::max(std::get<0>(volumeRamp), 0.0f);
    volumeInfo.targetVolume = std::max(std::get<1>(volumeRamp), 0.0f);
    volumeInfo.totalFrameNumber = static_cast<int>(
        static_cast<double>(volumeInfo.endTime - volumeInfo.startTime) / frameDuration);
    volumeInfo.totalFrameNumber =
        (volumeInfo.totalFrameNumber <= 0) ? 1 : volumeInfo.totalFrameNumber;
    if (FLOAT_EQUAL(std::get<0>(volumeRamp), std::get<1>(volumeRamp))) {
      // Fast mode, 1 frame to do fade
      volumeInfo.totalFrameNumber = 1;
    }
    volumeControlInfo.push_back(volumeInfo);
  }
}

void AudioSmoothVolume::seek(int64_t time) {
  currentVolumeIndex = 0;
  currentAudioVolume = 1.0f;
  updateGainGap = true;
  volumeGapInFrame = 0.0f;
  if (time < 0) {
    return;
  }
  for (auto& volumeControl : volumeControlInfo) {
    if (volumeControl.endTime < time) {
      currentVolumeIndex++;
      currentAudioVolume = volumeControl.targetVolume;
    } else if (volumeControl.startTime <= time) {
      currentAudioVolume = (volumeControl.targetVolume - volumeControl.startVolume) *
                               static_cast<float>(time - volumeControl.startTime) /
                               static_cast<float>(volumeControl.endTime - volumeControl.startTime) +
                           volumeControl.startVolume;
      volumeGapInFrame = (volumeControl.targetVolume - volumeControl.startVolume) /
                         static_cast<float>(volumeControl.totalFrameNumber);
      updateGainGap = false;
    }
  }
}

bool isBufferValid(const uint8_t* buffer, size_t byteSize) {
  return buffer != nullptr && byteSize > 0;
}

void AudioSmoothVolume::process(int64_t pts, const std::shared_ptr<ByteData>& data) {
  if (data == nullptr || !isBufferValid(data->data(), data->length()) || pts < 0) {
    return;
  }
  float currentGain = 1.0f;
  float targetGain = 1.0f;
  calcSmoothVolumeRange(pts, currentGain, targetGain);
  smoothVolumeProcessing(data->data(), static_cast<int>(data->length()), pcmOutputConfig->channels,
                         currentGain, targetGain);
}

void AudioSmoothVolume::calcSmoothVolumeRange(int64_t inputPts, float& currentGain,
                                              float& targetGain) {
  // 这里表示当前 pts 在首个 control info 的 start pts 之前
  // 或者已经在最后一个 control info 的 end pts 之后，
  // 为 keep 状态, targetAudioVol=currentAudioVol
  if (inputPts < volumeControlInfo[currentVolumeIndex].startTime ||
      inputPts > volumeControlInfo[volumeControlInfo.size() - 1].endTime) {
    currentGain = currentAudioVolume;
    targetGain = currentAudioVolume;
    return;
  }

  bool isGetActualVolumeRange = false;
  if (inputPts > volumeControlInfo[currentVolumeIndex].endTime) {
    currentVolumeIndex++;
    if (inputPts >= volumeControlInfo[currentVolumeIndex].startTime) {
      isGetActualVolumeRange = true;
      updateGainGap = true;
    } else {
      isGetActualVolumeRange = false;
    }
  } else if (inputPts >= volumeControlInfo[currentVolumeIndex].startTime) {
    isGetActualVolumeRange = true;
  }

  if (isGetActualVolumeRange) {
    if (!updateGainGap &&
        FLOAT_EQUAL(volumeControlInfo[currentVolumeIndex].targetVolume, currentAudioVolume)) {
      currentGain = currentAudioVolume;
      targetGain = currentAudioVolume;
      updateGainGap = true;
    } else {
      if (updateGainGap) {
        auto& volumeControl = volumeControlInfo[currentVolumeIndex];
        currentAudioVolume = volumeControl.startVolume;
        volumeGapInFrame = (volumeControl.targetVolume - currentAudioVolume) /
                           static_cast<float>(volumeControl.totalFrameNumber);
        updateGainGap = false;
      }
      // 这里不会出现精度溢出
      currentGain = currentAudioVolume;
      // 这里会可能出现精度溢出，要保护
      targetGain = std::max(currentAudioVolume + volumeGapInFrame, 0.0f);
      currentAudioVolume = targetGain;
    }
  } else {
    currentGain = currentAudioVolume;
    targetGain = currentAudioVolume;
    updateGainGap = true;
  }
}

void AudioSmoothVolume::smoothVolumeProcessing(uint8_t* audioBuffer, int byteSize,
                                               int channelNumber, float currentGain,
                                               float targetGain) {
  if (!isBufferValid(audioBuffer, byteSize)) {
    return;
  }

  if (FLOAT_EQUAL(currentGain, targetGain)) {
    if (currentGain == 1.0) {
      // Do nothing
      return;
    } else {
      gainApplyProcess(currentGain, audioBuffer, byteSize);
    }
  } else {
    int16_t nSampleNum = byteSize / 2;
    int16_t nSamplePerChannel = nSampleNum / channelNumber;
    auto fGainGap =
        static_cast<double>((targetGain - currentGain) / static_cast<float>(nSamplePerChannel));
    auto pAudioBuf = reinterpret_cast<int16_t*>(audioBuffer);
    int32_t nResult;
    if (FLOAT_EQUAL(fGainGap, 0.0f)) {
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

bool isGainValid(float gain) {
  return gain >= 0 && !FLOAT_EQUAL(gain, 1.0f);
}

void AudioSmoothVolume::gainApplyProcess(float gain, uint8_t* buffer, int byteSize) {
  if (!isGainValid(gain) && !isBufferValid(buffer, byteSize)) {
    return;
  }

  int16_t oldValue;
  int16_t newValue;
  int32_t mulValue;
  int32_t sampleNum = byteSize / 2;
  auto pBuf = reinterpret_cast<int16_t*>(buffer);
  float rounding;
  for (int i = 0; i < sampleNum; i += 2) {
    oldValue = pBuf[i];
    rounding = (oldValue >= 0) ? 0.5 : -0.5f;
    mulValue = static_cast<int32_t>(static_cast<float>(oldValue) * gain + rounding);
    if (mulValue > SHRT_MAX) {
      newValue = SHRT_MAX;
    } else if (mulValue < SHRT_MIN) {
      newValue = SHRT_MIN;
    } else {
      newValue = static_cast<int16_t>(mulValue);
    }
    pBuf[i] = newValue;

    oldValue = pBuf[i + 1];
    rounding = (oldValue >= 0) ? 0.5 : -0.5f;
    mulValue = static_cast<int32_t>(static_cast<float>(oldValue) * gain + rounding);
    if (mulValue > SHRT_MAX) {
      newValue = SHRT_MAX;
    } else if (mulValue < SHRT_MIN) {
      newValue = SHRT_MIN;
    } else {
      newValue = static_cast<int16_t>(mulValue);
    }
    pBuf[i + 1] = newValue;
  }
}
}  // namespace pag
#endif
