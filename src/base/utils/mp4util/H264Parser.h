//
// Created by katacai on 2022/3/8.
//

#pragma once
#include <sstream>
#include "ExpGolomb.h"
#include "H264Nalu.h"
#include "codec/utils/ByteArray.h"
#include "platform/Log.h"

namespace pag {
class H264Frame {
 public:
  H264Frame(std::vector<H264Nalu*> units, bool isKeyFrame = false);
  ~H264Frame();
  std::vector<H264Nalu*> units;
  bool isKeyFrame = false;
};

class SpsData {
 public:
  ByteData* sps;
  std::string codec;
  int width;
  int height;
};

class H264Parser {
 public:
  static SpsData parseSPS(H264Nalu* h264Nalu);
  static std::vector<ByteData*> getNaluFromSequence(VideoSequence& sequence);
  static std::vector<H264Frame*> getH264Frames(std::vector<ByteData*> nalus);
  static void parseHeader(H264Nalu* nalu);

 private:
  static std::pair<int, int> readSPS(H264Nalu* h264Nalu);
  static void skipScalingList(ExpGolomb& decoder, int count);
};
}  // namespace pag
