/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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
#include "tgfx/core/DataView.h"
namespace exporter {

inline uint32_t BitsToBytes(const uint64_t capacity) {
  return static_cast<uint32_t>((capacity + 7) >> 3);
}

class ByteArray final {
 public:
  ByteArray() = default;

  ByteArray(const uint8_t* data, size_t length)
      : dataView(data, length, tgfx::ByteOrder::BigEndian) {
  }

  /**
   * The number of bytes of data available for reading from the current position in the byte array to the end of
   * the array.
   */
  size_t bytesAvailable() const {
    return dataView.size() - _position;
  }

  bool empty() const {
    return dataView.isEmpty();
  }

  /**
   * The bytes of the ByteArray object.
   */
  const uint8_t* data() const {
    return dataView.bytes();
  }

  /**
   * The length of the ByteArray object.
   */
  size_t length() const {
    return dataView.size();
  }

  /**
   * Moves, or returns the current position, of the file pointer into the ByteArray object. This is the point at
   * which the next call to a read method starts reading.
   */
  size_t position() const {
    return _position;
  };

  void setPosition(size_t value);

  void skip(size_t size);

  /**
   * Reads a Boolean value from the byte stream. A signed 8-bit integer is read, and true is returned if the
   * integer is nonzero, false otherwise.
   */
  bool readBoolean();

  /**
  * Reads a signed 8-bit integer from the byte stream.
  */
  int8_t readInt8();

  /**
   * Reads a unsigned 8-bit integer from the byte stream.
   */
  uint8_t readUint8();

  /**
   * Reads a signed 16-bit integer from the byte stream.
   */
  int16_t readInt16();

  /**
   * Reads a unsigned 16-bit integer from the byte stream.
   */
  uint16_t readUint16();

  /**
   * Reads a signed 32-bit integer from the byte stream.
   */
  int32_t readInt32();

  /**
   * Reads a unsigned 32-bit integer from the byte stream.
   */
  uint32_t readUint32();

  /**
   * Reads a signed 64-bit integer from the byte stream.
   */
  int64_t readInt64();

  /**
   * Reads a unsigned 64-bit integer from the byte stream.
   */
  uint64_t readUint64();

  /**
   * Reads an IEEE 754 single-precision (32-bit) floating-point number from the byte stream.
   */
  float readFloat();

  /**
   * Reads an IEEE 754 double-precision (64-bit) floating-point number from the byte stream.
   */
  double readDouble();

  /**
   * Reads the number of data bytes, specified by the length parameter, from the byte stream.
   * @param length The number of bytes to read. The default value of 0 causes all available data to be read.
   */
  ByteArray readBytes(uint32_t length);

  /**
   * Reads a UTF-8 string from the byte stream. The string is assumed to be a sequential list of bytes terminated
   * by the null character byte.
   */
  std::string readUTF8String();

 private:
  tgfx::DataView dataView = {};
  size_t _position = 0;
  size_t _bitPosition = 0;

  void bitPositionChanged(size_t offset);

  void positionChanged(size_t offset);

  bool checkEndOfFile(size_t bytesToRead);
};

}  // namespace exporter
