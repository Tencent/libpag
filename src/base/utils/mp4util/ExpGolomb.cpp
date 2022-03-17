//
// Created by katacai on 2022/3/10.
//

#include "ExpGolomb.h"
namespace pag {
ExpGolomb::ExpGolomb(uint8_t* data, int size) : data(data) {
  index = 0;
  bitLength = size * 8;
}

ExpGolomb::~ExpGolomb() { data = nullptr; }

int ExpGolomb::bitsAvailable() { return bitLength - index; }

bool ExpGolomb::skipBits(int size) {
  if (bitsAvailable() < size) {
    return false;
  }
  index += size;
  return true;
}

uint8_t ExpGolomb::readBits(int size, bool moveIndex) {
  auto result = getBits(size, index, moveIndex);
  return result;
}

int ExpGolomb::skipLZ() {
  int leadingZeroCount = 0;
  for (leadingZeroCount = 0; leadingZeroCount < bitsAvailable(); ++leadingZeroCount) {
    if (getBits(1, index + leadingZeroCount, false) != 0) {
      index += leadingZeroCount;
      return leadingZeroCount;
    }
  }
  return leadingZeroCount;
}

uint8_t ExpGolomb::readUEG() {
  auto prefix = skipLZ();
  return readBits(prefix + 1) - 1;
}

uint8_t ExpGolomb::readBoolean() { return readBits(1) == 1; }

uint8_t ExpGolomb::readUByte(int numberOfBytes) { return readBits(numberOfBytes * 8); }

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

  //在当前字节中的偏移量
  auto offsetInCurrentByte = offsetBits % 8;
  //当前是在第几个字节
  auto byteIndex = offsetBits / 8;
  // 当前字节剩余的有效位数
  auto byte = data[byteIndex] & (0xff >> offsetInCurrentByte);
  //当前字节剩余位数
  auto bits = 8 - offsetInCurrentByte;
  if (bits >= size) {
    if (moveIndex) {
      index += size;
    }
    return byte >> (bits - size);
  }
  if (moveIndex) {
    index += bits;
  }
  auto nextSize = size - bits;
  return (byte << nextSize) | getBits(nextSize, offsetBits + bits, moveIndex);
}
}  // namespace pag
