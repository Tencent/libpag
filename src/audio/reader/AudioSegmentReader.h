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

#include "audio/model/AudioTrackSegment.h"
#include "audio/utils/AudioUtils.h"
#include "video/MediaDemuxer.h"

namespace pag {
class AudioSegmentReader {
 public:
  static std::shared_ptr<AudioSegmentReader> Make(AudioTrackSegment* segment,
                                                  std::shared_ptr<PCMOutputConfig> outputConfig);

  virtual ~AudioSegmentReader();

  void seekTo(int64_t time);

  SampleData copyNextSample();

 protected:
  AudioTrackSegment* segment;
  std::shared_ptr<PCMOutputConfig> outputConfig;
  uint8_t* emptyData = nullptr;
  int64_t emptyDataSize = 0;

  AudioSegmentReader(AudioTrackSegment* segment, std::shared_ptr<PCMOutputConfig> outputConfig);

  virtual bool openInternal() {
    return true;
  }

  virtual void seekToInternal(int64_t) {
  }

  virtual SampleData copyNextSampleInternal() {
    return {};
  }

 private:
  // segment 开始时间对应的 PCM offset
  int64_t startOffset = -1;
  // segment 结尾时间对应的 PCM offset
  int64_t endOffset = -1;
  // 当前输出过的 PCM offset
  int64_t currentOffset = -1;

  bool open();

  void initEmptyData();

  friend class AudioTrackReader;
};
}  // namespace pag

#endif
