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

#include "AudioSource.h"

namespace pag {

AudioSource::AudioSource(const std::string& filePath) : filePath(filePath) {
  auto demuxer = ffmovie::FFAudioDemuxer::Make(filePath);
  this->demuxer = std::move(demuxer);
}

AudioSource::AudioSource(std::shared_ptr<ByteData> data) : data(data) {
  auto demuxer = ffmovie::FFAudioDemuxer::Make(data->data(), data->length());
  this->demuxer = std::move(demuxer);
}

bool AudioSource::operator==(const AudioSource& source) {
  return !isEmpty() && (filePath == source.filePath || data == source.data);
}

bool AudioSource::isEmpty() const {
  return filePath.empty() && data == nullptr;
}

std::shared_ptr<ffmovie::FFAudioDemuxer> AudioSource::getDemuxer() const {
  return demuxer;
}

}  // namespace pag
