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

#include "URLAudio.h"
#include <mutex>
#include <unordered_map>
#include "audio/demux/AudioDemuxer.h"

namespace pag {
static std::mutex mutex{};
static std::unordered_map<std::string, std::shared_ptr<URLAudio>> fileAudioMap{};
static std::unordered_map<ByteData*, std::shared_ptr<URLAudio>> dataAudioMap{};

template <typename T>
std::shared_ptr<URLAudio> AudioFromSource(const AudioSource& source, T key,
                                          std::unordered_map<T, std::shared_ptr<URLAudio>>& map) {
  std::lock_guard<std::mutex> locker(mutex);
  auto iter = map.find(key);
  if (iter != map.end()) {
    return iter->second;
  }
  auto audio = std::make_shared<URLAudio>(source);
  if (audio->duration() <= 0 || audio->tracks().empty()) {
    return nullptr;
  }
  map.insert(std::make_pair(key, audio));
  return audio;
}

std::shared_ptr<URLAudio> URLAudio::Make(const AudioSource& source) {
  if (source.empty()) {
    return nullptr;
  }
  if (source.filePath.empty()) {
    return AudioFromSource(source, source.data.get(), dataAudioMap);
  }
  return AudioFromSource(source, source.filePath, fileAudioMap);
}

URLAudio::URLAudio(AudioSource source) : source(std::move(source)) {
}

void URLAudio::loadValues() {
  if (loaded) {
    return;
  }
  loaded = true;
  auto demuxer = AudioDemuxer::Make(source);
  if (demuxer == nullptr) {
    return;
  }
  for (unsigned int i = 0; i < demuxer->getTrackCount(); ++i) {
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
    auto track = std::shared_ptr<AudioTrack>(new AudioTrack(format->getInteger(KEY_TRACK_ID)));
    auto trackDuration = format->getLong(KEY_DURATION);
    auto timeRange = TimeRange{0, trackDuration};
    track->_segments.emplace_back(source, track->_trackID, timeRange, timeRange);

    _tracks.push_back(track);
  }
}
}  // namespace pag
#endif
