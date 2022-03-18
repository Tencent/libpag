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

#include "AudioComposition.h"

namespace pag {
std::shared_ptr<AudioCompositionTrack> AudioComposition::addTrack(int preferredTrackID) {
  auto newTrackID = preferredTrackID;
  if (newTrackID == TRACK_ID_INVALID) {
    newTrackID = generateTrackID();
  } else {
    auto iter = std::find_if(_tracks.begin(), _tracks.end(),
                             [newTrackID](const std::shared_ptr<AudioTrack>& mediaTrack) {
                               return mediaTrack->trackID() == newTrackID;
                             });
    if (iter != _tracks.end()) {
      newTrackID = generateTrackID();
    }
  }
  auto track = std::make_shared<AudioCompositionTrack>(newTrackID);
  _tracks.push_back(track);
  return track;
}

void AudioComposition::removeTrack(int byTrackID) {
  auto iter = std::find_if(_tracks.begin(), _tracks.end(),
                           [byTrackID](const std::shared_ptr<AudioTrack>& mediaTrack) {
                             return mediaTrack->trackID() == byTrackID;
                           });
  if (iter == _tracks.end()) {
    return;
  }
  _tracks.erase(iter);
}

void AudioComposition::removeAllTracks() {
  if (_tracks.empty()) {
    return;
  }
  _tracks.clear();
}

int AudioComposition::generateTrackID() {
  int trackID = 0;
  for (auto& track : _tracks) {
    trackID = std::max(trackID, track->trackID());
  }
  return ++trackID;
}
}  // namespace pag
#endif
