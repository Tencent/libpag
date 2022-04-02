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
#include <math.h>
#include <algorithm>
#include "H264Parser.h"
#include "Mp4Generator.h"
#include "SimpleArray.h"

#define SEQUENCE_NUMBER 1
#define BASE_MEDIA_DECODE_TIME 0
#define BASE_MEDIA_TIME_SCALE 6000

namespace pag {
class Mp4Flags {
 public:
  int isLeading = 0;
  int isDependedOn = 0;
  int hasRedundancy = 0;
  int degradPrio = 0;
  int isNonSync = 0;
  int dependsOn = 0;
  bool isKeyFrame = false;
};

class Mp4Sample {
 public:
  int index = 0;
  int size = 0;
  int32_t duration = 0;
  int32_t cts = 0;
  Mp4Flags flags;
};

class Mp4Track {
 public:
  Mp4Track();
  ~Mp4Track();

  int id;
  std::string type;
  int len;
  std::vector<ByteData*> sps;
  std::vector<ByteData*> pps;
  int width;
  int height;
  int timescale;
  int32_t duration;
  std::vector<std::shared_ptr<Mp4Sample>> samples;
  std::vector<int32_t> pts;
  std::string codec;
  float fps;
  int32_t implicitOffset;
};

extern int trackId;

class H264Remuxer {
 public:
  H264Remuxer();
  ~H264Remuxer();
  static int GetTrackID();
  static std::unique_ptr<H264Remuxer> Remux(VideoSequence& videoSequence);
  std::unique_ptr<ByteData> convertMp4();
  void writeMp4BoxesInSequence(VideoSequence& sequence);
  int getPayLoadSize();

  Mp4Track mp4Track;
  VideoSequence *videoSequence;

 private:
  static int32_t GetImplicitOffset(std::vector<VideoFrame*>& frames);
};
}  // namespace pag
