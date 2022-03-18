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

#include "AudioAsset.h"
#include "AudioTrackSegment.h"
#include "PCMAudio.h"
#include "URLAudio.h"

namespace pag {
std::shared_ptr<AudioAsset> AudioAsset::Make(const std::string& filePath) {
  return Make(AudioSource(filePath));
}

std::shared_ptr<AudioAsset> AudioAsset::Make(std::shared_ptr<ByteData> data) {
  return Make(AudioSource(std::move(data)));
}

std::shared_ptr<AudioAsset> AudioAsset::Make(std::shared_ptr<PAGPCMStream> stream) {
  return Make(AudioSource(std::move(stream)));
}

std::shared_ptr<AudioAsset> AudioAsset::Make(const AudioSource& source) {
  if (source.isPCMStream()) {
    return PCMAudio::Make(source);
  }
  return URLAudio::Make(source);
}

int64_t AudioAsset::duration() {
  int64_t duration = 0;
  for (auto& track : _tracks) {
    duration = std::max(duration, track->duration());
  }
  return duration;
}
}  // namespace pag
#endif
