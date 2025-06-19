/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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
#include <string>

namespace exporter {

bool CheckBigEndian();

static const bool IS_BIG_ENDIAN = CheckBigEndian();

union Bit8 {
  int8_t intValue;
  uint8_t uintValue;
  bool boolValue;
};
union Bit16 {
  uint8_t bytes[2];
  int16_t intValue;
  uint16_t uintValue;
};

union Bit32 {
  uint8_t bytes[4];
  int32_t intValue;
  uint32_t uintValue;
  float floatValue;
};

union Bit64 {
  uint8_t bytes[8];
  int64_t intValue;
  uint64_t uintValue;
  double doubleValue;
};

inline uint32_t BitsToBytes(const uint64_t capacity) {
  return static_cast<uint32_t>((capacity + 7) >> 3);
}

class ByteArray final {
 public:
  ByteArray() = default;

  ByteArray(const uint8_t* data, uint32_t length) : bytes(data), _length(length) {
  }

  /**
   * The number of bytes of data available for reading from the current position in the byte array to the end of
   * the array.
   */
  uint32_t bytesAvailable() const {
    return _length - _position;
  }

  bool empty() const {
    return _length == 0;
  }

  /**
   * The bytes of the ByteArray object.
   */
  const uint8_t* data() const {
    return bytes;
  }

  /**
   * The length of the ByteArray object.
   */
  uint32_t length() const {
    return _length;
  }

  /**
   * Moves, or returns the current position, of the file pointer into the ByteArray object. This is the point at
   * which the next call to a read method starts reading.
   */
  uint32_t position() const {
    return _position;
  };

  void setPosition(uint32_t value) {
    _position = value;
  }

  void skip(uint32_t size);

  /**
   * Reads a Boolean value from the byte stream. A signed 8-bit integer is read, and true is returned if the
   * integer is nonzero, false otherwise.
   */
  bool readBoolean() {
    return readBit8().boolValue;
  }

  /**
  * Reads a signed 8-bit integer from the byte stream.
  */
  int8_t readInt8() {
    return readBit8().intValue;
  }

  /**
   * Reads a unsigned 8-bit integer from the byte stream.
   */
  uint8_t readUint8() {
    return readBit8().uintValue;
  }

  /**
   * Reads a signed 16-bit integer from the byte stream.
   */
  int16_t readInt16() {
    return readBit16().intValue;
  }

  /**
   * Reads a unsigned 16-bit integer from the byte stream.
   */
  uint16_t readUint16() {
    return readBit16().uintValue;
  }

  /**
   * Reads a signed 32-bit integer from the byte stream.
   */
  int32_t readInt32() {
    return readBit32().intValue;
  }

  /**
   * Reads a unsigned 32-bit integer from the byte stream.
   */
  uint32_t readUint32() {
    return readBit32().uintValue;
  }

  /**
   * Reads a signed 64-bit integer from the byte stream.
   */
  int64_t readInt64() {
    return readBit64().intValue;
  }

  /**
   * Reads a unsigned 64-bit integer from the byte stream.
   */
  uint64_t readUint64() {
    return readBit64().uintValue;
  }

  /**
   * Reads an IEEE 754 single-precision (32-bit) floating-point number from the byte stream.
   */
  float readFloat() {
    return readBit32().floatValue;
  }

  /**
   * Reads an IEEE 754 double-precision (64-bit) floating-point number from the byte stream.
   */
  double readDouble() {
    return readBit64().doubleValue;
  }

  /**
   * Reads the number of data bytes, specified by the length parameter, from the byte stream.
   * @param length The number of bytes to read. The default value of 0 causes all available data to be read.
   */
  ByteArray readBytes(uint32_t length = UINT32_MAX);

  /**
   * Reads a UTF-8 string from the byte stream. The string is assumed to be a sequential list of bytes terminated
   * by the null character byte.
   */
  std::string readUTF8String();

 private:
  Bit8 readBit8();
  Bit16 readBit16();
  Bit32 readBit32();
  Bit64 readBit64();

  const uint8_t* bytes = nullptr;
  uint32_t _length = 0;
  uint32_t _position = 0;
};
}  // namespace exporter
