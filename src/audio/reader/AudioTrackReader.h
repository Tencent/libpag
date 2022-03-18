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

#include "AudioSegmentReader.h"
#include "audio/model/AudioTrack.h"
#include "audio/process/AudioSmoothVolume.h"

namespace pag {
class AudioTrackReader {
 public:
  explicit AudioTrackReader(std::shared_ptr<AudioTrack> track,
                            std::shared_ptr<PCMOutputConfig> outputConfig);

  ~AudioTrackReader();

  void seekTo(int64_t targetTime);

  std::shared_ptr<ByteData> copyNextSample();

 private:
  // 当前读到的时间，用来判断是否切换 segment
  int64_t readTime = 0;
  // 已经输出过的 PCM 长度
  int64_t outputtedLength = 0;
  // 输出 PCM 长度的粒度
  int64_t outSampleLength = 0;
  // 缓存池大小
  int64_t sampleCacheLength = 0;
  std::shared_ptr<AudioTrack> track;
  std::shared_ptr<PCMOutputConfig> outputConfig;
  // 当前使用的 segmentReader
  std::shared_ptr<AudioSegmentReader> reader = nullptr;
  // PCM 缓存池，存到一定数量才往外输出，结尾的时候不管有多少都输出
  uint8_t* cacheData = nullptr;
  // 当前缓存池有多少 PCM 数据
  int64_t cacheSize = 0;
  AudioSmoothVolume* smoothVolume = nullptr;

  void checkReader();

  std::shared_ptr<ByteData> copyNextSampleInternal();

  friend class AudioReader;
};
}  // namespace pag

#endif
