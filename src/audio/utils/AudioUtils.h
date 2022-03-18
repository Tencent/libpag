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

#ifdef __cplusplus
extern "C" {
#endif

#include <libswresample/swresample.h>

#ifdef __cplusplus
}
#endif

#include "audio/model/AudioSource.h"
#include "pag/types.h"

#define DEFAULT_OUTPUT_SAMPLE_COUNT 1024

namespace pag {
struct VolumeRange {
  VolumeRange() = default;

  VolumeRange(const TimeRange timeRange, float startVolume, float endVolume)
      : timeRange(timeRange), startVolume(startVolume), endVolume(endVolume) {
  }

  TimeRange timeRange{-1, -1};
  float startVolume = 1.0f;
  float endVolume = 1.0f;
};

struct PCMOutputConfig {
  // 采样率，默认 44.1kHZ
  int sampleRate = 44100;
  int format = AV_SAMPLE_FMT_S16;
  // 默认按每声道 1024 个采样点往外输出
  int outputSamplesCount = DEFAULT_OUTPUT_SAMPLE_COUNT;
  // 双声道
  int channels = 2;
  // 立体声
  uint64_t channelLayout = AV_CH_LAYOUT_STEREO;
};

inline int64_t SampleLengthToCount(int64_t length, PCMOutputConfig* config) {
  return length / config->channels /
         av_get_bytes_per_sample(static_cast<AVSampleFormat>(config->format));
}

inline int64_t SampleLengthToTime(int64_t length, PCMOutputConfig* config) {
  return ceill(length * 1000000.0 / config->channels /
               av_get_bytes_per_sample(static_cast<AVSampleFormat>(config->format)) /
               config->sampleRate);
}

inline int64_t SampleCountToLength(int64_t sampleCount, PCMOutputConfig* config) {
  return sampleCount * config->channels *
         av_get_bytes_per_sample(static_cast<AVSampleFormat>(config->format));
}

inline int64_t SampleTimeToLength(int64_t time, PCMOutputConfig* config) {
  return SampleCountToLength(ceill(static_cast<double>(time * config->sampleRate) / 1000000.0),
                             config);
}

inline bool operator==(const PCMOutputConfig& left, const AVFrame& right) {
  return left.sampleRate == right.sample_rate && left.format == right.format &&
         left.channels == right.channels && left.channelLayout == right.channel_layout;
}

inline bool operator!=(const PCMOutputConfig& left, const AVFrame& right) {
  return !(left == right);
}
}  // namespace pag

#endif
