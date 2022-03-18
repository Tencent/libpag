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

#include "AudioTrackSegmentReader.h"

namespace pag {
AudioTrackSegmentReader::AudioTrackSegmentReader(AudioTrackSegment* segment,
                                                 std::shared_ptr<PCMOutputConfig> outputConfig)
    : AudioSegmentReader(segment, std::move(outputConfig)) {
}

AudioTrackSegmentReader::~AudioTrackSegmentReader() {
  delete shifting;
  delete reader;
}

void AudioTrackSegmentReader::seekToInternal(int64_t time) {
  auto source = segment->timeMapping.source;
  auto target = segment->timeMapping.target;
  auto sourceTime = static_cast<int64_t>(static_cast<double>(time - target.start) *
                                             static_cast<double>(source.duration()) /
                                             static_cast<double>(target.duration()) +
                                         static_cast<double>(source.start));
  inputEOS = false;
  shifting = nullptr;
  reader->seek(sourceTime);
}

bool AudioTrackSegmentReader::openInternal() {
  reader = AudioSourceReader::Make(segment->source, segment->sourceTrackID, outputConfig).release();
  if (reader == nullptr) {
    return false;
  }
  reader->seek(segment->timeMapping.source.start);
  speed = static_cast<float>(segment->timeMapping.source.duration()) /
          static_cast<float>(segment->timeMapping.target.duration());
  return true;
}

SampleData AudioTrackSegmentReader::copyNextSampleInternal() {
  if (speed == 1) {
    return reader->copyNextSample();
  }
  if (shifting == nullptr) {
    shifting = new AudioShifting(outputConfig);
    shifting->setSpeed(speed);
  }
  int ret = 0;
  while (!inputEOS && ret <= 0) {
    auto data = reader->copyNextSample();
    if (data.empty()) {
      ret = shifting->sendInputEOS();
      inputEOS = true;
    } else {
      ret = shifting->sendAudioBytes(data);
    }
  }
  return shifting->readAudioBytes();
}
}  // namespace pag
#endif
