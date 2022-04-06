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

#include <cmath>
#include <memory>
#include <string>
#include <vector>
#include "H264Parser.h"
#include "Mp4Generator.h"
#include "SimpleArray.h"

namespace pag {
struct Mp4Flags {
  int isLeading = 0;
  int isDependedOn = 0;
  int hasRedundancy = 0;
  int degradPrio = 0;
  int isNonSync = 0;
  int dependsOn = 0;
  bool isKeyFrame = false;
};

struct Mp4Sample {
  int index = 0;
  int size = 0;
  int32_t duration = 0;
  int32_t cts = 0;
  Mp4Flags flags;
};

struct Mp4Track {
  int id = 0;
  std::string type = "video";
  int len = 0;
  std::vector<ByteData*> sps;
  std::vector<ByteData*> pps;
  int width = 0;
  int height = 0;
  int timescale = 0;
  int32_t duration = 0;
  std::vector<std::shared_ptr<Mp4Sample>> samples;
  std::vector<int32_t> pts;
  std::string codec;
  float fps = 0.0f;
  int32_t implicitOffset = 0;
};

class H264Remuxer {
 public:
  static std::unique_ptr<H264Remuxer> Remux(VideoSequence* videoSequence);
  int getTrackID();
  [[nodiscard]] int getPayLoadSize() const;
  std::unique_ptr<ByteData> convertMp4();
  void writeMp4BoxesInSequence(VideoSequence* sequence);

 private:
  static Frame GetImplicitOffset(const std::vector<VideoFrame*>& frames);

  int trackId = 0;
  Mp4Track mp4Track;
  VideoSequence* videoSequence = nullptr;
};
}  // namespace pag
