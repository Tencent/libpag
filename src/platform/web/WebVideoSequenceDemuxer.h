/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include <emscripten/val.h>
#include "rendering/sequences/VideoSequenceDemuxer.h"

using namespace emscripten;

namespace pag {
class WebVideoSequenceDemuxer : public VideoSequenceDemuxer {
 public:
  WebVideoSequenceDemuxer(std::shared_ptr<File> file, VideoSequence* sequence,
                          PAGFile* pagFile = nullptr)
      : VideoSequenceDemuxer(std::move(file), sequence, pagFile) {
  }

  VideoSample nextSample() override;

  int64_t getSampleTimeAt(int64_t targetTime) override;

  bool needSeeking(int64_t currentTime, int64_t targetTime) override;

  void seekTo(int64_t) override;

  void reset() override;

  val getStaticTimeRanges();

  std::unique_ptr<ByteData> getMp4Data();

  void setForHardwareDecoder(bool value) {
    hardwareBacked = value;
  }

 private:
  bool hardwareBacked = false;
};
}  // namespace pag
