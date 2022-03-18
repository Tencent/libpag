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

#include <libavformat/avformat.h>
#include <libavutil/avutil.h>

#ifdef __cplusplus
}
#endif

#include "audio/utils/AudioUtils.h"
#include "sonic.h"
#include "video/FFmpegDemuxer.h"

namespace pag {
class AudioShifting {
 public:
  explicit AudioShifting(std::shared_ptr<PCMOutputConfig> pcmOutputConfig);

  ~AudioShifting();

  void setSpeed(float speed);

  void setVolume(float volume);

  void setPitch(float pitch);

  int sendAudioBytes(const SampleData& audioData);

  int sendInputEOS();

  SampleData readAudioBytes();

 private:
  sonicStream stream = nullptr;
  std::shared_ptr<PCMOutputConfig> pcmOutputConfig = nullptr;
  int16_t* outPCMBuffer = nullptr;
};
}  // namespace pag

#endif
