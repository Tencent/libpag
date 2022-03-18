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

#include "AudioTrackReader.h"
#include "audio/model/AudioAsset.h"

namespace pag {
class AudioReader {
 public:
  static std::shared_ptr<PCMOutputConfig> DefaultConfig();

  static std::shared_ptr<AudioReader> Make(
      const std::shared_ptr<AudioAsset>& audio,
      std::shared_ptr<PCMOutputConfig> outputConfig = AudioReader::DefaultConfig());

  ~AudioReader();

  void seekTo(int64_t time);

  /**
   * 按照设置的 outputSampleCount 输出 PCM，如果轨道没有数据，就返回 0
   * 由于实时添加删除轨道的功能，所以 reader 没有结束的标识
   * 调用方自己决定是否到结尾
   */
  std::shared_ptr<PAGAudioFrame> copyNextFrame();

  void onAudioChanged();

  std::shared_ptr<PCMOutputConfig> outputConfig() {
    return _outputConfig;
  }

 private:
  std::shared_ptr<AudioAsset> audio = nullptr;
  std::shared_ptr<PCMOutputConfig> _outputConfig;
  // 当前已经输出的 PCM 长度
  int64_t currentOutputLength = 0;
  // 当前已经输出的 PCM 时长
  int64_t currentTime = 0;
  // 轨道的 reader
  std::list<std::shared_ptr<AudioTrackReader>> readers{};
  // 输出 PCM 的内存空间，只申请一次
  uint8_t* outputBuffer = nullptr;
  int64_t outputBufferSize = 0;

  explicit AudioReader(std::shared_ptr<PCMOutputConfig> outputConfig);

  int64_t copyNextSample();
};
}  // namespace pag

#endif
