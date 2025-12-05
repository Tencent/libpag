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

#include "AudioAsset.h"
#include <memory>

namespace pag {
std::shared_ptr<AudioAsset> AudioAsset::Make(const std::string& filePath) {
  auto audio = std::make_shared<AudioAsset>();
  audio->setSource(std::make_shared<AudioSource>(filePath));
  return audio;
}

std::shared_ptr<AudioAsset> AudioAsset::Make(std::shared_ptr<ByteData> data) {
  auto audio = std::make_shared<AudioAsset>();
  audio->setSource(std::make_shared<AudioSource>(std::move(data)));
  return audio;
}

std::shared_ptr<AudioAsset> AudioAsset::Make(std::shared_ptr<AudioSource> source) {
  auto audio = std::make_shared<AudioAsset>();
  audio->setSource(std::move(source));
  return audio;
}

int64_t AudioAsset::duration() {
  int64_t duration = 0;
  for (auto& track : tracks) {
    duration = std::max(duration, track->duration());
  }
  return duration;
}

std::vector<std::shared_ptr<AudioTrack>> AudioAsset::getTracks() {
  return tracks;
}

std::shared_ptr<AudioSource> AudioAsset::getSource() {
  return source;
}

std::shared_ptr<AudioTrack> AudioAsset::addTrack(int preferredTrackID) {
  auto newTrackID = preferredTrackID;
  if (newTrackID == -1) {
    newTrackID = generateTrackID();
  } else {
    auto iter =
        std::find_if(tracks.begin(), tracks.end(), [newTrackID](std::shared_ptr<AudioTrack> track) {
          return track->getTrackID() == newTrackID;
        });
    if (iter != tracks.end()) {
      newTrackID = generateTrackID();
    }
  }
  auto track = std::make_shared<AudioTrack>(newTrackID);
  tracks.push_back(track);
  return track;
}

void AudioAsset::setSource(std::shared_ptr<AudioSource> source) {
  tracks.clear();
  this->source = std::move(source);
  loadValues();
}

void AudioAsset::loadValues() {
  if (source == nullptr || source->isEmpty()) {
    return;
  }
  auto demuxer = source->getDemuxer();
  if (demuxer == nullptr) {
    return;
  }
  for (int i = 0; i < demuxer->getTrackCount(); ++i) {
    auto format = demuxer->getTrackFormat(i);
    if (format == nullptr) {
      continue;
    }
    auto mime = format->getString(KEY_MIME);
    if (mime.empty()) {
      continue;
    }
    if (mime.find("audio") == std::string::npos) {
      continue;
    }
    auto track = std::make_shared<AudioTrack>(format->getInteger(KEY_TRACK_ID));
    auto trackDuration = format->getLong(KEY_DURATION);
    auto timeRange = TimeRange{0, trackDuration};
    track->addSegment({source, track->getTrackID(), timeRange, timeRange});
    tracks.push_back(track);
  }
}

void AudioAsset::removeTrack(int trackID) {
  auto iter =
      std::find_if(tracks.begin(), tracks.end(), [trackID](std::shared_ptr<AudioTrack> mediaTrack) {
        return mediaTrack->getTrackID() == trackID;
      });
  if (iter == tracks.end()) {
    return;
  }
  tracks.erase(iter);
}

void AudioAsset::removeAllTracks() {
  tracks.clear();
}

int AudioAsset::generateTrackID() {
  int trackID = 0;
  for (auto& track : tracks) {
    trackID = std::max(trackID, track->getTrackID());
  }
  return ++trackID;
}

}  // namespace pag
