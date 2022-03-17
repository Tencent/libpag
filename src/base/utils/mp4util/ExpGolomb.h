//
// Created by katacai on 2022/3/10.
//
#pragma once
#include "pag/types.h"

namespace pag {
class ExpGolomb {
 public:
  ExpGolomb(uint8_t* data, int si);
  ~ExpGolomb();
  int bitsAvailable();
  bool skipBits(int size);
  uint8_t readBits(int size, bool moveIndex = true);
  uint8_t readUEG();
  uint8_t readBoolean();
  uint8_t readUByte(int numberOfBytes = 1);
  int readUE();
  int readEG();

 private:
  /**
   * 跳过数值为 0 的位数
   * @return 跳过了多少个 0 位
   */
  int skipLZ();

  /**
   * 获取指定位数的数值
   * @param size 位数
   * @param offsetBits 位偏移量
   * @param moveIndex 是否移动当前位指向
   * @return 位对应的数值
   */
  int getBits(int size, int offsetBits, bool moveIndex = true);

  uint8_t* data;
  int index;
  int bitLength;
};
}  // namespace pag
