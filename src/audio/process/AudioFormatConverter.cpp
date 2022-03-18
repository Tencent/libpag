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

#include "AudioFormatConverter.h"

namespace pag {
AudioFormatConverter::AudioFormatConverter(std::shared_ptr<PCMOutputConfig> pcmOutputConfig)
    : pcmOutputConfig(std::move(pcmOutputConfig)) {
}

AudioFormatConverter::~AudioFormatConverter() {
  av_freep(&pConvertBuff);
  swr_free(&pSwrContext);
}

SampleData AudioFormatConverter::convert(AVFrame* frame) {
  if (frame == nullptr) {
    return {};
  }
  if (preFramePCMOutputConfig == nullptr) {
    preFramePCMOutputConfig = std::make_shared<PCMOutputConfig>();
  }
  int nbSample =
      static_cast<int>(frame->nb_samples * pcmOutputConfig->sampleRate / frame->sample_rate) + 256;
  if (nbSample > outputSamples) {
    av_freep(&pConvertBuff);
    pConvertBuff = nullptr;
    outputSamples = nbSample;
  }
  if (pConvertBuff == nullptr) {
    if (av_samples_alloc(&pConvertBuff, nullptr, pcmOutputConfig->channels, outputSamples,
                         static_cast<AVSampleFormat>(pcmOutputConfig->format), 0) < 0) {
      return {};
    }
  }
  if (*preFramePCMOutputConfig != *frame) {
    swr_free(&pSwrContext);
    pSwrContext = nullptr;
  }
  if (!pSwrContext) {
    preFramePCMOutputConfig->sampleRate = frame->sample_rate;
    preFramePCMOutputConfig->format = frame->format;
    preFramePCMOutputConfig->channels = frame->channels;
    preFramePCMOutputConfig->channelLayout = frame->channel_layout;
    pSwrContext = swr_alloc_set_opts(nullptr, pcmOutputConfig->channelLayout,
                                     static_cast<AVSampleFormat>(pcmOutputConfig->format),
                                     pcmOutputConfig->sampleRate, frame->channel_layout,
                                     static_cast<AVSampleFormat>(frame->format), frame->sample_rate,
                                     0, nullptr);
    swr_init(pSwrContext);
  }

  auto newNbSamples = swr_convert(pSwrContext, &pConvertBuff, outputSamples,
                                  (const uint8_t**)frame->data, frame->nb_samples);
  if (newNbSamples <= 0) {
    return {};
  }
  return {pConvertBuff, SampleCountToLength(newNbSamples, pcmOutputConfig.get())};
}
}  // namespace pag
#endif
