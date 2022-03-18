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

#include "audio/model/AudioClip.h"
#include "audio/model/AudioComposition.h"

namespace pag {
PAGAudio::PAGAudio() {
  audioComposition = std::make_shared<AudioComposition>();
}

int PAGAudio::addTracks(PAGComposition* fromComposition) {
  std::lock_guard<std::mutex> autoLock(locker);
  auto trackIDs = AudioClip::DumpTracks(fromComposition, audioComposition.get());
  if (trackIDs.empty()) {
    return TRACK_ID_INVALID;
  }
  auto key = trackIDs.front();
  trackIDMap.insert(std::make_pair(key, trackIDs));
  setAudioChanged(true);
  return key;
}

int PAGAudio::addTrack(const PAGAudioTrack& track) {
  if (track.track->_segments.empty()) {
    return TRACK_ID_INVALID;
  }
  std::lock_guard<std::mutex> autoLock(locker);
  auto audioTrack = audioComposition->addTrack();
  auto trackID = audioTrack->_trackID;
  *audioTrack = *track.track;
  audioTrack->_trackID = trackID;
  setAudioChanged(true);
  return trackID;
}

void PAGAudio::removeTrack(int byTrackID) {
  if (byTrackID < 0) {
    return;
  }
  std::lock_guard<std::mutex> autoLock(locker);
  auto iter = trackIDMap.find(byTrackID);
  if (iter == trackIDMap.end()) {
    audioComposition->removeTrack(byTrackID);
  } else {
    for (auto& trackID : (*iter).second) {
      audioComposition->removeTrack(trackID);
    }
    trackIDMap.erase(iter);
  }
  setAudioChanged(true);
}

void PAGAudio::removeAllTracks() {
  std::lock_guard<std::mutex> autoLock(locker);
  audioComposition->removeAllTracks();
  trackIDMap.clear();
  setAudioChanged(true);
}

bool PAGAudio::isEmpty() {
  std::lock_guard<std::mutex> autoLock(locker);
  return audioComposition->tracks().empty();
}

void PAGAudio::setAudioChanged(bool changed) {
  for (auto reader : readers) {
    reader->audioChanged = changed;
  }
}

void PAGAudio::addReader(PAGAudioReader* reader) {
  readers.push_back(reader);
}

void PAGAudio::removeReader(PAGAudioReader* reader) {
  auto iter = std::find(readers.begin(), readers.end(), reader);
  if (iter != readers.end()) {
    readers.erase(iter);
  }
}
}  // namespace pag
#endif
