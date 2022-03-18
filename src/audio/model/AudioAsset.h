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

#include "AudioTrack.h"

namespace pag {
class AudioAsset {
 public:
  virtual ~AudioAsset() = default;

  static std::shared_ptr<AudioAsset> Make(const std::string& filePath);

  static std::shared_ptr<AudioAsset> Make(std::shared_ptr<ByteData> data);

  static std::shared_ptr<AudioAsset> Make(std::shared_ptr<PAGPCMStream> stream);

  static std::shared_ptr<AudioAsset> Make(const AudioSource& source);

  virtual int64_t duration();

  /**
   * 只存放音频轨道，没有视频的
   */
  virtual std::vector<std::shared_ptr<AudioTrack>> tracks() {
    return _tracks;
  }

 protected:
  std::vector<std::shared_ptr<AudioTrack>> _tracks{};
};
}  // namespace pag

#endif
