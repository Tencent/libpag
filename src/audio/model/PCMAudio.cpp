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

#include "PCMAudio.h"

namespace pag {
std::shared_ptr<PCMAudio> PCMAudio::Make(const AudioSource& source) {
  if (source.empty() || !source.isPCMStream()) {
    return nullptr;
  }
  return std::make_shared<PCMAudio>(source);
}

void PCMAudio::loadValues() {
  if (loaded) {
    return;
  }
  loaded = true;
  auto track = std::shared_ptr<AudioTrack>(new AudioTrack(0));
  auto timeRange = TimeRange{0, source.stream->duration()};
  track->_segments.emplace_back(source, track->_trackID, timeRange, timeRange);
  _tracks.push_back(track);
}
}  // namespace pag

#endif
