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

#include "ExpGolomb.h"

namespace pag {
ExpGolomb::ExpGolomb(ByteData* data) : data(data) {
  bitLength = data ? static_cast<int>(data->length()) * 8 : 0;
}

void ExpGolomb::reset(ByteData* byteData) {
  data = byteData;
  position = 0;
  bitLength = data ? static_cast<int>(data->length()) * 8 : 0;
}

int ExpGolomb::bitsAvailable() {
  return bitLength - position;
}

bool ExpGolomb::skipBits(int size) {
  if (bitsAvailable() < size) {
    return false;
  }
  position += size;
  return true;
}

bool ExpGolomb::skipBytes(int size) {
  return skipBits(size * 8);
}

uint8_t ExpGolomb::readBits(int size, bool moveIndex) {
  return getBits(size, position, moveIndex);
}

int ExpGolomb::skipLZ() {
  int leadingZeroCount = 0;
  for (; leadingZeroCount < bitsAvailable(); ++leadingZeroCount) {
    if (getBits(1, position + leadingZeroCount, false) != 0) {
      position += leadingZeroCount;
      return leadingZeroCount;
    }
  }
  return leadingZeroCount;
}

uint8_t ExpGolomb::readUEG() {
  return readBits(skipLZ() + 1) - 1;
}

uint8_t ExpGolomb::readBoolean() {
  return readBits(1) == 1;
}

uint8_t ExpGolomb::readUByte(int numberOfBytes) {
  return readBits(numberOfBytes * 8);
}

int ExpGolomb::readUE() {
  auto r = 0;
  auto i = 0;
  while (readBits(1) == 0 && i < 32 && bitsAvailable()) {
    i += 1;
  }
  r = readBits(i);
  r += (1 << i) - 1;
  return r;
}

int ExpGolomb::readEG() {
  int value = readUEG();
  if (0x01 & value) {
    return (1 + value) >> 1;
  }
  return -1 * (value >> 1);
}

int ExpGolomb::getBits(int size, int offsetBits, bool moveIndex) {
  if (bitsAvailable() < size) {
    return 0;
  }

  auto offsetInCurrentByte = offsetBits % 8;
  auto byteIndex = offsetBits / 8;
  auto byte = data->data()[byteIndex] & (0xff >> offsetInCurrentByte);
  auto bits = 8 - offsetInCurrentByte;
  if (bits >= size) {
    if (moveIndex) {
      position += size;
    }
    return byte >> (bits - size);
  }
  if (moveIndex) {
    position += bits;
  }
  auto nextSize = size - bits;
  return (byte << nextSize) | getBits(nextSize, offsetBits + bits, moveIndex);
}
}  // namespace pag
