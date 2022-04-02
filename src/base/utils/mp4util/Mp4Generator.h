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

#include <chrono>
#include <map>
#include <vector>
#include "SimpleArray.h"

namespace pag {
class Mp4Track;

class BoxParam {
 public:
  BoxParam() = default;
  void copy(const BoxParam& boxParam);

  int offset = 0;
  int timescale = 0;
  int32_t duration = 0;
  int sequenceNumber = 0;
  int nalusBytesLen = 0;
  int32_t baseMediaDecodeTime = 0;
  Mp4Track* track = nullptr;
  VideoSequence* videoSequence = nullptr;
  std::vector<Mp4Track*> tracks;
};

typedef int (*WriteStreamFun)(SimpleArray* stream, bool write);

class Mp4Generator {
 public:
  static int FTYP(SimpleArray* stream, bool write = false);
  static int MOOV(SimpleArray* stream, bool write = false);
  static int MOOF(SimpleArray* stream, bool write = false);
  static int MDAT(SimpleArray* stream, bool write = false);
  static int MVHD(SimpleArray* stream, bool write = false);
  static int MVEX(SimpleArray* stream, bool write = false);
  static int MFHD(SimpleArray* stream, bool write = false);
  static int TRAF(SimpleArray* stream, bool write = false);
  static int MDHD(SimpleArray* stream, bool write = false);
  static int MDIA(SimpleArray* stream, bool write = false);
  static int MINF(SimpleArray* stream, bool write = false);
  static int STBL(SimpleArray* stream, bool write = false);
  static int TREX(SimpleArray* stream, bool write = false);
  static int SDTP(SimpleArray* stream, bool write = false);
  static int AVC1(SimpleArray* stream, bool write = false);
  static int STSD(SimpleArray* stream, bool write = false);
  static int TKHD(SimpleArray* stream, bool write = false);
  static int TRAK(SimpleArray* stream, bool write = false);
  static int EDTS(SimpleArray* stream, bool write = false);
  static int ELST(SimpleArray* stream, bool write = false);
  static int TRUN(SimpleArray* stream, bool write = false);
  static int STTS(SimpleArray* stream, bool write = false);
  static int CTTS(SimpleArray* stream, bool write = false);
  static int STSS(SimpleArray* stream, bool write = false);
  static int SMHD(SimpleArray* stream, bool write = false);
  static int VMHD(SimpleArray* stream, bool write = false);
  static int STSC(SimpleArray* stream, bool write = false);
  static int STSZ(SimpleArray* stream, bool write = false);
  static int STCO(SimpleArray* stream, bool write = false);
  static int AVCC(SimpleArray* stream, bool write = false);
  static int TFHD(SimpleArray* stream, bool write = false);
  static int TFDT(SimpleArray* stream, bool write = false);
  static int DREF(SimpleArray* stream, bool write = false);
  static int HDLR(SimpleArray* stream, bool write = false);
  static int DINF(SimpleArray* stream, bool write = false);
  static void Clear();
  static void InitParam(const BoxParam& boxParam);

 private:
  static int WriteCharCode(SimpleArray* stream, std::string stringData, bool write = false);
  static int WriteH264Nalus(SimpleArray* stream, bool write = false);
  static int Box(SimpleArray* stream, std::string type, std::vector<WriteStreamFun>& boxFunctions,
                 bool write = false);
  static int32_t GetNowTime();

  static std::map<std::string, int> BoxSizeMap;
  static BoxParam param;
};
}  // namespace pag
