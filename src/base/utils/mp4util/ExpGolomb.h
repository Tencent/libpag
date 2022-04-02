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

#include "pag/types.h"

namespace pag {
class ExpGolomb {
 public:
  explicit ExpGolomb(ByteData* data);
  int bitsAvailable();
  bool skipBits(int size);
  bool skipBytes(int size);
  void reset(ByteData* byteData);
  uint8_t readUEG();
  uint8_t readBoolean();
  uint8_t readUByte(int numberOfBytes = 1);
  uint8_t readBits(int size, bool moveIndex = true);
  int readUE();
  int readEG();

 private:
  /**
   * Skips zero bits.
   */
  int skipLZ();

  int getBits(int size, int offsetBits, bool moveIndex = true);

  ByteData* data = nullptr;
  int position = 0;
  int bitLength = 0;
};
}  // namespace pag
