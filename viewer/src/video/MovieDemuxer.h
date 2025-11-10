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

#pragma once

#include <unordered_map>
#include "base/utils/Log.h"
#include "codec/NALUType.h"
#include "ffmovie/movie.h"
#include "pag/types.h"
#include "rendering/video/VideoDemuxer.h"

namespace pag {

class MovieDemuxer : public VideoDemuxer {
 public:
  static std::unique_ptr<MovieDemuxer> Make(const std::string& path, NALUType startCodeType);

  bool advance();
  bool needSeeking(int64_t currentTime, int64_t targetTime) override;
  void reset() override;
  void seekTo(int64_t timestamp) override;
  void selectTrack(int trackID);
  int getTrackID() const;
  int getRotation() const;
  int getTrackCount();
  int64_t getSampleTimeAt(int64_t targetTime) override;
  int64_t getSampleTime();
  VideoFormat getFormat() override;
  VideoSample nextSample() override;

 private:
  explicit MovieDemuxer(std::unique_ptr<ffmovie::FFVideoDemuxer> demuxer);

  int trackID = 0;
  int rotation = 0;
  std::unique_ptr<ffmovie::FFVideoDemuxer> demuxer = nullptr;
};
}  // namespace pag
