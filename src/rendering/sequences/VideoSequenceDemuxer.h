/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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
#include "pag/pag.h"
#include "rendering/video/VideoDemuxer.h"

namespace pag {
class VideoSequenceDemuxer : public VideoDemuxer {
 public:
  VideoSequenceDemuxer(std::shared_ptr<File> file, VideoSequence* sequence,
                       PAGFile* pagFile = nullptr);

  VideoFormat getFormat() override {
    return format;
  }

  VideoSample nextSample() override;

  int64_t getSampleTimeAt(int64_t targetTime) override;

  bool needSeeking(int64_t currentTime, int64_t targetTime) override;

  void seekTo(int64_t targetTime) override;

  void reset() override;

 protected:
  Frame sampleIndex = 0;
  VideoSequence* sequence = nullptr;
  Frame maxPTSFrame = -1;

 private:
  // Keep a reference to the File in case the Sequence object is released while we are using it.
  std::shared_ptr<File> file = nullptr;
  // used by the video decoder on the web platform.
  PAGFile* pagFile = nullptr;
  VideoFormat format = {};
  std::vector<Frame> keyframes = {};

  bool staticContent() const override {
    return sequence->composition->staticContent();
  }

  friend class HardwareDecoder;
};
}  // namespace pag
