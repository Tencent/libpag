/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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
#include "codec/utils/StreamContext.h"
#include "pag/types.h"
#include "tgfx/core/DataView.h"

namespace pag {

/**
 * The DecodeStream class provides methods and properties to optimize reading and working with
 * binary data. The DecodeStream object does not take ownership of the binary data, The caller must
 * keep the data alive until the DecodeStream object is done with it. Note: The byte order of
 * DecodeStream is always little-endian.
 */
class DecodeStream final {
 public:
  explicit DecodeStream(StreamContext* context) : context(context) {
  }

  DecodeStream(StreamContext* context, const uint8_t* data, uint32_t length)
      : context(context), dataView(data, length) {
  }

  /**
   * This DecodeStream's byte order. The byte order is used when reading or writing multibyte
   * values. The order of a newly-created stream is always ByteOrder::LittleEndian.
   */
  tgfx::ByteOrder byteOrder() const {
    return dataView.byteOrder();
  }

  /**
   * Set this DecodeStream's byte order.
   */
  void setByteOrder(tgfx::ByteOrder order) {
    dataView.setByteOrder(order);
  }

  /**
   * The number of the byte data available for reading from the current position in the stream to
   * the end of the stream.
   */
  uint32_t bytesAvailable() const {
    return static_cast<uint32_t>(dataView.size() - _position);
  }

  const uint8_t* data() const {
    return dataView.bytes();
  }

  /**
   * The length of the DecodeStream object.
   */
  uint32_t length() const {
    return static_cast<uint32_t>(dataView.size());
  }

  /**
   * Moves or returns the current position, of the file pointer into the DecodeStream object. This
   * is the point at which the next call to a read method starts reading.
   */
  uint32_t position() const {
    return static_cast<uint32_t>(_position);
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
   * Reads an encoded signed 32-bit integer from the byte stream.
   */
  int32_t readEncodedInt32();

  /**
   * Reads an encoded unsigned 32-bit integer from the byte stream.
   */
  uint32_t readEncodedUint32();

  /**
   * Reads an encoded signed 64-bit integer from the byte stream.
   */
  int64_t readEncodedInt64();

  /**
   * Reads an encoded unsigned 64-bit integer from the byte stream.
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
   * Reads a list of Point2D values from the stream.
   */
  void readPoint2DList(Point* points, uint32_t count, float precision);

  /**
   * Reads a list of Point3D values from the stream.
   */
  void readPoint3DList(Point3D* points, uint32_t count, float precision);

  /**
   * Reads a Boolean value from the byte stream. A 1-bit integer is read, and true is returned if
   * the integer is nonzero, false otherwise.
   */
  bool readBitBoolean() {
    return readUBits(1) != 0;
  }

  mutable StreamContext* context;

 private:
  tgfx::DataView dataView = {};
  size_t _position = 0;
  size_t _bitPosition = 0;

  void bitPositionChanged(size_t offset);

  void positionChanged(size_t offset);

  bool checkEndOfFile(uint32_t bytesToRead);
};
}  // namespace pag
