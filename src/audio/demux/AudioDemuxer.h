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

#pragma once
#ifdef FILE_MOVIE

#ifdef __cplusplus
extern "C" {
#endif

#include <libavformat/avformat.h>

#ifdef __cplusplus
}
#endif

#include "audio/model/AudioSource.h"
#include "video/FFmpegDemuxer.h"
#include "video/MediaFormat.h"

namespace pag {
class AudioDemuxer {
 public:
  static std::unique_ptr<AudioDemuxer> Make(const AudioSource& source);

  ~AudioDemuxer();

  bool selectTrack(unsigned int index);

  unsigned int getTrackCount();

  MediaFormat* getTrackFormat(unsigned int index);

  AVStream* getStream(unsigned int index);

  bool seekTo(int64_t timestamp);

  bool advance();

  SampleData readSampleData() const;

  int64_t sampleTime() const;

  struct BufferData {
    uint8_t* ptr = nullptr;  // 文件中对应位置指针
    size_t size = 0;         // 文件当前指针到末尾的距离
    uint8_t* originPtr = nullptr;
    size_t fileSize = 0;
  } bufferData;

 private:
  AudioSource mediaSource{};
  AVIOContext* avioCtx = nullptr;
  int currentStreamIndex = -1;
  int64_t currentTime = -1;
  AVFormatContext* fmtCtx = nullptr;
  AVPacket avPacket{};
  std::unordered_map<unsigned int, MediaFormat*> formats{};

  AudioDemuxer() = default;

  int openFile(const AudioSource& source);

  MediaFormat* getTrackFormatInternal(unsigned int index);
};
}  // namespace pag

#endif
