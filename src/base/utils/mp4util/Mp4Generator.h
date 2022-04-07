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

#include <string>
#include <unordered_map>
#include <vector>
#include "SimpleArray.h"
#include "pag/file.h"

namespace pag {
struct Mp4Track;

struct BoxParam {
  int offset = 0;
  int timescale = 0;
  int32_t duration = 0;
  int sequenceNumber = 0;
  int nalusBytesLen = 0;
  int32_t baseMediaDecodeTime = 0;
  Mp4Track* track = nullptr;
  const VideoSequence* videoSequence = nullptr;
  std::vector<Mp4Track*> tracks;
};

class Mp4Generator {
 public:
  explicit Mp4Generator(BoxParam param);

  int ftyp(SimpleArray* stream, bool write = false);
  int moov(SimpleArray* stream, bool write = false);
  int moof(SimpleArray* stream, bool write = false);
  int mdat(SimpleArray* stream, bool write = false);

 private:
  int mvhd(SimpleArray* stream, bool write = false);
  int mvex(SimpleArray* stream, bool write = false);
  int mfhd(SimpleArray* stream, bool write = false);
  int traf(SimpleArray* stream, bool write = false);
  int mdhd(SimpleArray* stream, bool write = false);
  int mdia(SimpleArray* stream, bool write = false);
  int minf(SimpleArray* stream, bool write = false);
  int stbl(SimpleArray* stream, bool write = false);
  int trex(SimpleArray* stream, bool write = false);
  int sdtp(SimpleArray* stream, bool write = false);
  int avc1(SimpleArray* stream, bool write = false);
  int stsd(SimpleArray* stream, bool write = false);
  int tkhd(SimpleArray* stream, bool write = false);
  int trak(SimpleArray* stream, bool write = false);
  int edts(SimpleArray* stream, bool write = false);
  int elst(SimpleArray* stream, bool write = false);
  int trun(SimpleArray* stream, bool write = false);
  int stts(SimpleArray* stream, bool write = false);
  int ctts(SimpleArray* stream, bool write = false);
  int stss(SimpleArray* stream, bool write = false);
  int smhd(SimpleArray* stream, bool write = false);
  int vmhd(SimpleArray* stream, bool write = false);
  int stsc(SimpleArray* stream, bool write = false);
  int stsz(SimpleArray* stream, bool write = false);
  int stco(SimpleArray* stream, bool write = false);
  int avcc(SimpleArray* stream, bool write = false);
  int tfhd(SimpleArray* stream, bool write = false);
  int tfdt(SimpleArray* stream, bool write = false);
  int dref(SimpleArray* stream, bool write = false);
  int hdlr(SimpleArray* stream, bool write = false);
  int dinf(SimpleArray* stream, bool write = false);
  int writeH264Nalus(SimpleArray* stream, bool write = false) const;
  int box(SimpleArray* stream, const std::string& type,
          const std::vector<std::function<int(SimpleArray*, bool)>>& boxFunctions,
          bool write = false);

  std::unordered_map<std::string, int> boxSizeMap;
  BoxParam param;
};

}  // namespace pag
