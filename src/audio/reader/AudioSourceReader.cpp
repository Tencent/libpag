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

#include "AudioSourceReader.h"
#include "audio/decode/AudioDecoder.h"
#include "audio/demux/AudioDemuxer.h"

#define MAX_TRY_DECODE_COUNT 100

// 由于 mp3 的 bit reservoir 技术，需要往前多 seek 几帧，60ms 大概 3 帧
#define SEEK_FORWARD_TIME_US 60000

namespace pag {
class PCMStreamReader : public AudioSourceReader {
 public:
  explicit PCMStreamReader(std::shared_ptr<PAGPCMStream> stream) : stream(std::move(stream)) {
  }

  void seek(int64_t toTime) override {
    stream->seek(toTime);
  }

  SampleData copyNextSample() override {
    auto data = stream->copyNextFrame();
    if (data == nullptr) {
      return {};
    }
    return {data->data, data->length};
  }

 private:
  std::shared_ptr<PAGPCMStream> stream = nullptr;

  bool valid() override {
    return stream != nullptr;
  }
};

class AudioFileReader : public AudioSourceReader {
 public:
  AudioFileReader(const AudioSource& source, int trackID,
                  std::shared_ptr<PCMOutputConfig> outputConfig)
      : _outputConfig(std::move(outputConfig)) {
    demuxer = AudioDemuxer::Make(source).release();
    if (demuxer == nullptr) {
      return;
    }
    demuxer->selectTrack(trackID);
    decoder = AudioDecoder::Make(demuxer->getStream(trackID), _outputConfig).release();
  }

  ~AudioFileReader() override {
    delete demuxer;
    delete decoder;
  }

  void seek(int64_t toTime) override {
    startTime = toTime;
    startOffset = SampleTimeToLength(toTime, _outputConfig.get());
    advance = true;
    decoder->onFlush();
    demuxer->seekTo(std::max(static_cast<int64_t>(0), toTime - SEEK_FORWARD_TIME_US));
  }

  SampleData copyNextSample() override {
    SampleData data{};
    int tryDecodeCount = 0;
    do {
      if (!sendData()) {
        return data;
      }
      auto result = decoder->onDecodeFrame();
      if (result == DecodingResult::Success) {
        data = decoder->onRenderFrame();
        auto time = decoder->currentPresentationTime();
        if (0 <= time && time < startTime) {
          size_t delta = startOffset - SampleTimeToLength(time, _outputConfig.get());
          if (delta < data.length) {
            data.data += delta;
            data.length -= delta;
          } else {
            data.data = nullptr;
            data.length = 0;
          }
        }
      } else if (result == DecodingResult::TryAgainLater) {
        if ((tryDecodeCount++) >= MAX_TRY_DECODE_COUNT) {
          return {};
        }
      } else {
        return data;
      }
    } while (data.empty());
    return data;
  }

 private:
  AudioDemuxer* demuxer = nullptr;
  AudioDecoder* decoder = nullptr;
  int64_t startTime = -1;
  int64_t startOffset = -1;
  bool advance = false;
  std::shared_ptr<PCMOutputConfig> _outputConfig;

  bool valid() override {
    return demuxer != nullptr && decoder != nullptr;
  }

  bool sendData() {
    if (advance) {
      advance = false;
      demuxer->advance();
    }
    auto sampleData = demuxer->readSampleData();
    if (sampleData.empty()) {
      auto result = decoder->onEndOfStream();
      if (result == DecodingResult::Error) {
        return false;
      }
    } else {
      auto result = decoder->onSendBytes(sampleData.data, sampleData.length, demuxer->sampleTime());
      if (result == DecodingResult::Error) {
        return false;
      } else if (result == DecodingResult::Success) {
        advance = true;
      }
    }
    return true;
  }
};

std::unique_ptr<AudioSourceReader> AudioSourceReader::Make(
    const AudioSource& source, int trackID, std::shared_ptr<PCMOutputConfig> outputConfig) {
  AudioSourceReader* reader;
  if (source.isPCMStream()) {
    reader = new PCMStreamReader(source.stream);
  } else {
    reader = new AudioFileReader(source, trackID, std::move(outputConfig));
  }
  if (!reader->valid()) {
    delete reader;
    return nullptr;
  }
  return std::unique_ptr<AudioSourceReader>(reader);
}
}  // namespace pag

#endif
