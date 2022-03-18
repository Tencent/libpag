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

#pragma once

#include <unordered_map>
#include "base/utils/Log.h"
#include "codec/NALUType.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/parseutils.h>

#ifdef __cplusplus
}
#endif

#include "MediaDemuxer.h"
#include "MediaFormat.h"
#include "pag/types.h"

namespace pag {
class FFmpegDemuxer : public MediaDemuxer {
 public:
  static std::unique_ptr<FFmpegDemuxer> Make(const std::string& filePath, NALUType startCodeType);
  ~FFmpegDemuxer() override;

  void seekTo(int64_t timeUs) override;
  bool selectTrack(int index);
  int64_t getSampleTime() override;
  int getTrackCount();
  bool advance() override;
  SampleData readSampleData() override;
  MediaFormat* getTrackFormat(int index);

 private:
  AVFormatContext* formatContext = nullptr;
  int currentStreamIndex = -1;
  int videoStreamIndex = -1;
  AVPacket avPacket = {};
  AVRational avRational = {};
  int64_t currentTime = 0;
  std::unordered_map<int, MediaFormat*> formats{};
  const AVBitStreamFilter* absFilter = nullptr;
  AVBSFContext* avbsfContext = nullptr;

  NALUType naluStartCodeType = NALUType::AVCC;

  FFmpegDemuxer() = default;
  bool open(const std::string& filePath);
  MediaFormat* getTrackFormatInternal(int index);
  void getNALUInfoWithVideoStream(MediaFormat* trackFormat, AVStream* avStream);
  std::shared_ptr<PTSDetail> createPTSDetail() override;
};
}  // namespace pag

#endif