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

#include <string>
#include "ByteOrder.h"
#include "codec/utils/StreamContext.h"
#include "pag/types.h"

namespace pag {

/**
 * The DecodeStream class provides methods and properties to optimize reading and working with
 * binary data. The DecodeStream object does not take ownership of the of the the binary data, The
 * caller must keep the data alive until the DecodeStream object is done with it. Note: The byte
 * order of DecodeStream is little-endian by default.
 */
class DecodeStream final {
 public:
  explicit DecodeStream(StreamContext* context) : context(context) {
  }

  DecodeStream(StreamContext* context, const uint8_t* data, uint32_t length)
      : context(context), bytes(data), _length(length) {
  }

  /**
   * This DecodeStream's byte order. The byte order is used when reading or writing multibyte
   * values. The order of a newly-created stream is always ByteOrder::LittleEndian.
   */
  ByteOrder order() const;

  /**
   * Modifies this DecodeStream's byte order.
   */
  void setOrder(ByteOrder order);

  /**
   * The number of bytes of data available for reading from the current position in the stream to
   * the end of the stream.
   */
  uint32_t bytesAvailable() const {
    return _length - _position;
  }

  const uint8_t* data() const {
    return bytes;
  }

  /**
   * The length of the DecodeStream object.
   */
  uint32_t length() const {
    return _length;
  }

  /**
   * Moves, or returns the current position, of the file pointer into the DecodeStream object. This
   * is the point at which the next call to a read method starts reading.
   */
  uint32_t position() const {
    return _position;
  }

  void setPosition(uint32_t value);

  void skip(uint32_t numBytes);

  void alignWithBytes() {
    _bitPosition = static_cast<uint64_t>(_position) * 8;
  }

  /**
   * Reads a Boolean value from the byte stream. A signed 8-bit integer is read, and true is
   * returned if the integer is nonzero, false otherwise.
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
   * Reads a unsigned 24-bit integer from the byte stream.
   */
  uint32_t readUint24() {
    return readBit24().uintValue;
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
   * @param length The number of bytes to read.
   */
  DecodeStream readBytes(uint32_t length);

  /**
   * Reads a ByteData object from the byte stream.
   */
  std::unique_ptr<ByteData> readByteData();

  /**
   * Reads a UTF-8 string from the byte stream. The string is assumed to be a sequential list of
   * bytes terminated by the null character byte.
   */
  std::string readUTF8String();

  /**
   * Reads a encoded signed 32-bit integer from the byte stream.
   */
  int32_t readEncodedInt32();

  /**
   * Reads a encoded unsigned 32-bit integer from the byte stream.
   */
  uint32_t readEncodedUint32();

  /**
   * Reads a encoded signed 64-bit integer from the byte stream.
   */
  int64_t readEncodedInt64();

  /**
   * Reads a encoded unsigned 64-bit integer from the byte stream.
   */
  uint64_t readEncodedUint64();

  /**
   * Reads the specified number of bits as an signed integer from the bit stream.
   */
  int32_t readBits(uint8_t numBits);

  /**
   * Reads the specified number of bits as an unsigned integer from the bit stream.
   */
  uint32_t readUBits(uint8_t numBits);

  uint8_t readNumBits() {
    return static_cast<uint8_t>(readUBits(LENGTH_FOR_STORE_NUM_BITS) + 1);
  }

  /**
   * Reads a list of int32_t values from the stream.
   */
  void readInt32List(int32_t* values, uint32_t count);

  /**
   * Reads a list of uint32_t values from the stream.
   */
  void readUint32List(uint32_t* values, uint32_t count);

  /**
   * Reads a list of float values from the stream.
   */
  void readFloatList(float* values, uint32_t count, float precision);

  /**
   * Reads a Boolean value from the byte stream. A 1-bit integer is read, and true is returned if
   * the integer is nonzero, false otherwise.
   */
  bool readBitBoolean() {
    return readUBits(1) != 0;
  }

  mutable StreamContext* context;

 private:
  void bitPositionChanged() {
    _position = BitsToBytes(_bitPosition);
  }

  void positionChanged() {
    _bitPosition = static_cast<uint64_t>(_position) * 8;
  }

  Bit8 readBit8();
  Bit16 readBit16();
  Bit32 readBit24();
  Bit32 readBit32();
  Bit64 readBit64();

  ByteOrder _order = ByteOrder::LittleEndian;
  const uint8_t* bytes = nullptr;
  uint32_t _length = 0;
  uint32_t _position = 0;
  uint64_t _bitPosition = 0;
};
}  // namespace pag
