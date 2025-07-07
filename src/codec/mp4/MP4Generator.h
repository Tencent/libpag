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

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "codec/utils/EncodeStream.h"
#include "pag/file.h"

namespace pag {
struct MP4Flags {
  int isLeading = 0;
  int isDependedOn = 0;
  int hasRedundancy = 0;
  int degradPrio = 0;
  int isNonSync = 0;
  int dependsOn = 0;
  bool isKeyFrame = false;
};

struct MP4Sample {
  int index = 0;
  int size = 0;
  int32_t duration = 0;
  int32_t cts = 0;
  MP4Flags flags;
};

struct MP4Track {
  int id = 0;
  std::string type = "video";
  int len = 0;
  std::vector<ByteData*> sps;
  std::vector<ByteData*> pps;
  int width = 0;
  int height = 0;
  int timescale = 0;
  int32_t duration = 0;
  std::vector<std::shared_ptr<MP4Sample>> samples;
  std::vector<int32_t> pts;
  int32_t implicitOffset = 0;
};

struct BoxParam {
  int offset = 0;
  int timescale = 0;
  int32_t duration = 0;
  int sequenceNumber = 0;
  int nalusBytesLen = 0;
  int32_t baseMediaDecodeTime = 0;
  std::shared_ptr<MP4Track> track = nullptr;
  const VideoSequence* videoSequence = nullptr;
  std::vector<std::shared_ptr<MP4Track>> tracks;
};

class MP4Generator {
 public:
  explicit MP4Generator(BoxParam param);

  int ftyp(EncodeStream* stream, bool write = false);
  int moov(EncodeStream* stream, bool write = false);
  int moof(EncodeStream* stream, bool write = false);
  int mdat(EncodeStream* stream, bool write = false);

 private:
  int mvhd(EncodeStream* stream, bool write = false);
  int mvex(EncodeStream* stream, bool write = false);
  int mfhd(EncodeStream* stream, bool write = false);
  int traf(EncodeStream* stream, bool write = false);
  int mdhd(EncodeStream* stream, bool write = false);
  int mdia(EncodeStream* stream, bool write = false);
  int minf(EncodeStream* stream, bool write = false);
  int stbl(EncodeStream* stream, bool write = false);
  int trex(EncodeStream* stream, bool write = false);
  int sdtp(EncodeStream* stream, bool write = false);
  int avc1(EncodeStream* stream, bool write = false);
  int stsd(EncodeStream* stream, bool write = false);
  int tkhd(EncodeStream* stream, bool write = false);
  int trak(EncodeStream* stream, bool write = false);
  int edts(EncodeStream* stream, bool write = false);
  int elst(EncodeStream* stream, bool write = false);
  int trun(EncodeStream* stream, bool write = false);
  int stts(EncodeStream* stream, bool write = false);
  int ctts(EncodeStream* stream, bool write = false);
  int stss(EncodeStream* stream, bool write = false);
  int smhd(EncodeStream* stream, bool write = false);
  int vmhd(EncodeStream* stream, bool write = false);
  int stsc(EncodeStream* stream, bool write = false);
  int stsz(EncodeStream* stream, bool write = false);
  int stco(EncodeStream* stream, bool write = false);
  int avcc(EncodeStream* stream, bool write = false);
  int tfhd(EncodeStream* stream, bool write = false);
  int tfdt(EncodeStream* stream, bool write = false);
  int dref(EncodeStream* stream, bool write = false);
  int hdlr(EncodeStream* stream, bool write = false);
  int dinf(EncodeStream* stream, bool write = false);
  int writeH264Nalus(EncodeStream* stream, bool write = false) const;
  int box(EncodeStream* stream, const std::string& type,
          const std::vector<std::function<int(EncodeStream*, bool)>>& boxFunctions,
          bool write = false);

  std::unordered_map<std::string, int> boxSizeMap;
  BoxParam param;
};

}  // namespace pag
