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

#include <climits>
#include "pag/file.h"
#include "video/MediaDemuxer.h"

namespace pag {
class VideoSequenceDemuxer : public MediaDemuxer {
 public:
  explicit VideoSequenceDemuxer(VideoSequence* sequence);

  void seekTo(int64_t timeUs) override;

  int64_t getSampleTime() override;

  bool advance() override;

  SampleData readSampleData() override;

 private:
  VideoSequence* sequence;
  int seekFrameIndex = INT_MIN;
  int currentFrameIndex = 0;

  std::shared_ptr<PTSDetail> createPTSDetail() override;
};
}  // namespace pag
