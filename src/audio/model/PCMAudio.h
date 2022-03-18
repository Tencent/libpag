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

#include "AudioAsset.h"

namespace pag {
class PCMAudio : public AudioAsset {
 public:
  static std::shared_ptr<PCMAudio> Make(const AudioSource& source);

  explicit PCMAudio(AudioSource source) : source(std::move(source)) {
  }

  int64_t duration() override {
    loadValues();
    return AudioAsset::duration();
  }

  std::vector<std::shared_ptr<AudioTrack>> tracks() override {
    loadValues();
    return AudioAsset::tracks();
  }

 private:
  AudioSource source;
  bool loaded = false;

  void loadValues();
};
}  // namespace pag

#endif
