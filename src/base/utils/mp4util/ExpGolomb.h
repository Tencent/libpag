//
// Created by katacai on 2022/3/10.
//
#pragma once
#include "pag/types.h"

namespace pag {
class ExpGolomb {
 public:
  ExpGolomb(ByteData* data);
  ~ExpGolomb();
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
   * skip zero bits
   * @return count of skip step
   */
  int skipLZ();

  int getBits(int size, int offsetBits, bool moveIndex = true);

  ByteData* data;
  int index;
  int bitLength;
};
}  // namespace pag
