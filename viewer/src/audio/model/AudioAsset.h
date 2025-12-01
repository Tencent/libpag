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

#pragma once

#include "AudioTrack.h"
#include "pag/types.h"

namespace pag {

class AudioAsset {
 public:
  static std::shared_ptr<AudioAsset> Make(const std::string& filePath);
  static std::shared_ptr<AudioAsset> Make(std::shared_ptr<ByteData> data);
  static std::shared_ptr<AudioAsset> Make(std::shared_ptr<AudioSource> source);

  int64_t duration();
  std::vector<std::shared_ptr<AudioTrack>> getTracks();
  std::shared_ptr<AudioSource> getSource();
  std::shared_ptr<AudioTrack> addTrack(int preferredTrackID = -1);
  void setSource(std::shared_ptr<AudioSource> source);
  void loadValues();
  void removeTrack(int trackID);
  void removeAllTracks();

 private:
  int generateTrackID();

  std::shared_ptr<AudioSource> source = nullptr;
  std::vector<std::shared_ptr<AudioTrack>> tracks = {};
};

}  // namespace pag
