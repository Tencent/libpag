//
// Created by katacai on 2022/3/8.
//

#pragma once
#include <sstream>
#include "ExpGolomb.h"
#include "SimpleArray.h"
#include "pag/file.h"
#include "base/utils/Log.h"

namespace pag {
class SpsData {
 public:
  SpsData() = default;
  ~SpsData();
  ByteData* sps;
  std::string codec;
  int width;
  int height;
};

class H264Parser {
 public:
  static SpsData ParseSPS(ByteData* sysBytes);

 private:
  static std::pair<int, int> ReadSPS(ByteData* spsBytes);
  static void SkipScalingList(ExpGolomb& decoder, int count);
};
}  // namespace pag
