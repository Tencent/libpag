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

#include "AudioSourceReader.h"
#include "utils/AudioUtils.h"

namespace pag {

constexpr int64_t MaxTryDecodeCount = 100;
constexpr int64_t SeekForwardTimeUs = 60000;

AudioSourceReader::AudioSourceReader(std::shared_ptr<AudioSource> source, int trackID,
                                     std::shared_ptr<AudioOutputConfig> outputConfig)
    : trackID(trackID), source(source), outputConfig(outputConfig) {
  if (source == nullptr) {
    return;
  }
  demuxer = source->getDemuxer();
  if (demuxer == nullptr) {
    return;
  }
  demuxer->selectTrack(this->trackID);
  auto decoder = ffmovie::FFAudioDecoder::Make(demuxer.get(), std::move(outputConfig));
  this->decoder = std::move(decoder);
}

bool AudioSourceReader::isValid() {
  return demuxer != nullptr && decoder != nullptr;
}

void AudioSourceReader::seek(int64_t time) {
  startTime = time;
  startOffset = Utils::SampleTimeToLength(time, outputConfig->sampleRate, outputConfig->channels);
  advance = true;
  decoder->onFlush();
  demuxer->seekTo(std::max(static_cast<int64_t>(0), time - SeekForwardTimeUs));
}

SampleData AudioSourceReader::getNextFrame() {
  SampleData data = {};
  int tryDecodeCount = 0;
  do {
    if (!sendData()) {
      break;
    }
    auto result = decoder->onDecodeFrame();
    if (result == ffmovie::DecoderResult::Success) {
      data = decoder->onRenderFrame();
      auto time = decoder->currentPresentationTime();
      if (time >= 0 && time < startTime) {
        size_t delta = startOffset - Utils::SampleTimeToLength(time, outputConfig->sampleRate,
                                                               outputConfig->channels);
        if (delta < data.length) {
          data.data += delta;
          data.length -= delta;
        } else {
          data.data = nullptr;
          data.length = 0;
        }
      }
    } else if (result == ffmovie::DecoderResult::TryAgainLater) {
      if ((tryDecodeCount++) >= MaxTryDecodeCount) {
        return {};
      }
    } else {
      break;
    }
  } while (data.empty());
  return data;
}

bool AudioSourceReader::sendData() {
  if (advance) {
    advance = false;
    demuxer->advance();
  }
  auto sampleData = demuxer->readSampleData();
  if (sampleData.empty()) {
    auto result = decoder->onEndOfStream();
    if (result == ffmovie::DecoderResult::Error) {
      return false;
    }
  } else {
    auto result =
        decoder->onSendBytes(sampleData.data, sampleData.length, demuxer->getSampleTime());
    if (result == ffmovie::DecoderResult::Error) {
      return false;
    }
    if (result == ffmovie::DecoderResult::Success) {
      advance = true;
    }
  }
  return true;
}

}  // namespace pag
