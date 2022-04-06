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

#include "H264Parser.h"
#include "codec/utils/DecodeStream.h"

namespace pag {
SpsData H264Parser::ParseSps(ByteData* sysBytes) {
  DecodeStream byteArray(nullptr, sysBytes->data(), sysBytes->length());
  byteArray.skip(4);
  byteArray.skip(1);

  std::string numberStr = "0123456789ABCDEF";
  std::string codec = "avc1.";
  for (int i = 0; i < 3; ++i) {
    uint8_t num = byteArray.readUint8();
    std::string out;
    out.push_back(numberStr[(num >> 4) & 0xF]);
    out.push_back(numberStr[num & 0xF]);

    if (out.size() < 2) {
      out = "0" + out;
    }
    codec += out;
  }

  SpsData spsData;
  spsData.sps = sysBytes;
  spsData.codec = codec;
  return spsData;
}
}  // namespace pag
