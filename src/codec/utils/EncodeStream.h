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

#include "ByteOrder.h"
#include "codec/utils/StreamContext.h"
#include "pag/file.h"

namespace pag {

/**
 * The EncodeStream class provides methods and properties to optimize writing and working with
 * binary data. Note: The byte order of EncodeStream is always little-endian.
 */
class EncodeStream final {
 public:
  explicit EncodeStream(StreamContext* context, uint32_t capacity = 128);

  ~EncodeStream();

  /**
   * This EncodeStream's byte order. The byte order is used when reading or writing multibyte
   * values. The order of a newly-created stream is always ByteOrder::LittleEndian.
   */
  ByteOrder order() const;

  /**
   * Modifies this EncodeStream's byte order.
   */
  void setOrder(ByteOrder order);

  /**
   * Call the this method to take ownership of the current bytes. Once the release method is called,
   * the EncodeStream object will be reset to a new one, and the returned bytes will be managed by
   * the ByteData object.
   */
  std::unique_ptr<ByteData> release();

  /**
   * The length of the EncodeStream object.
   */
  uint32_t length() const {
    return _length;
  }

  /**
   * Moves, or returns the current position, of the file pointer into the EncodeStream object. This
   * is the point at which the next call to a write method starts writing.
   */
  uint32_t position() const {
    return _position;
  }

  void setPosition(uint32_t value) {
    _position = value;
    positionChanged();
  }

  void alignWithBytes() {
    _bitPosition = static_cast<uint64_t>(_position) * 8;
  }

  /**
   * Writes a Boolean value. A signed 8-bit integer is written according to the value parameter,
   * either 1 if true or 0 if false.
   */
  void writeBoolean(bool value);

  /**
   * Writes a 8-bit signed integer to the byte stream.
   */
  void writeInt8(int8_t value);

  /**
   * Writes a unsigned 8-bit integer to the byte stream.
   */
  void writeUint8(uint8_t value);

  /**
   * Writes a 16-bit signed integer to the byte stream.
   */
  void writeInt16(int16_t value);

  /**
   * Writes a unsigned 16-bit integer to the byte stream.
   */
  void writeUint16(uint16_t value);

  /**
   * Writes a unsigned 24-bit integer to the byte stream.
   */
  void writeUint24(uint32_t value);

  /**
   * Writes a 32-bit signed integer to the byte stream.
   */
  void writeInt32(int32_t value);

  /**
   * Writes a unsigned 32-bit integer to the byte stream.
   */
  void writeUint32(uint32_t value);

  /**
   * Writes a 64-bit signed integer to the byte stream.
   */
  void writeInt64(int64_t value);

  /**
   * Writes a unsigned 64-bit integer to the byte stream.
   */
  void writeUint64(uint64_t value);

  /**
   * Writes an IEEE 754 single-precision (32-bit) floating-point number to the byte stream.
   */
  void writeFloat(float value);

  /**
   * Writes an IEEE 754 double-precision (64-bit) floating-point number to the byte stream.
   */
  void writeDouble(double value);

  /**
   * Writes a sequence of length bytes from the specified stream, bytes, starting
   * offset(zero-based index) bytes into the byte stream. If the length parameter is omitted, the
   * default length of 0 is used; the method writes the entire stream starting at offset. If the
   * offset parameter is also omitted, the entire stream is written. If offset or length is out of
   * range, they are clamped to the beginning and end of the stream. Note: The position of target
   * EncodeStream will not change.
   * @param stream The EncodeStream object.
   * @param offset A zero-based index indicating the position into the stream to begin writing.
   * @param length An unsigned integer indicating how far into the stream to write.
   */
  void writeBytes(EncodeStream* stream, uint32_t length = 0, uint32_t offset = 0);

  void writeBytes(uint8_t* stream, uint32_t length, uint32_t offset = 0);

  /**
   * Writes a ByteData object to the byte stream.
   */
  void writeByteData(const ByteData* byteData);

  /**
   * Writes a UTF-8 string to the byte stream. The bytes representing the characters of the string
   * are written first, followed by the null character byte.
   */
  void writeUTF8String(const std::string& text);

  /**
   * Writes a encoded signed 32-bit integer to the byte stream.
   */
  void writeEncodedInt32(int32_t value) {
    writeEncodedInt64(value);
  }

  /**
   * Writes a encoded unsigned 32-bit integer to the byte stream.
   */
  void writeEncodedUint32(uint32_t value) {
    writeEncodedUint64(value);
  }

  /**
   * Writes a encoded signed 64-bit integer to the byte stream.
   */
  void writeEncodedInt64(int64_t value);

  /**
   * Writes a encoded unsigned 64-bit integer to the byte stream.
   */
  void writeEncodedUint64(uint64_t value);

  /**
   * Writes the signed bit and numBits-1 of a signed integer's lower bits to the bit stream.
   */
  void writeBits(int32_t value, uint8_t numBits);
  /**
   * Writes the specified number of a unsigned integer's lower bits to the bit stream.
   */
  void writeUBits(uint32_t value, uint8_t numBits);

  /**
   * Writes a list of int32_t values to the stream.
   */
  void writeInt32List(const int32_t* values, uint32_t count);

  /**
   * Writes a list of uint32_t values to the stream.
   */
  void writeUint32List(const uint32_t* values, uint32_t count);

  /**
   * Writes a list of float values to the stream.
   */
  void writeFloatList(const float* values, uint32_t count, float precision);

  /**
   * Writes a Boolean value. A 1-bit integer is written according to the value parameter, either 1
   * if true or 0 if false.
   */
  void writeBitBoolean(bool value) {
    writeUBits(value ? 1 : 0, 1);
  }

  mutable StreamContext* context;

 private:
  void ensureCapacity(uint32_t length) {
    if (length > capacity) {
      expandCapacity(length);
    }
  }

  void bitPositionChanged() {
    _position = BitsToBytes(_bitPosition);
    if (_position > _length) {
      _length = _position;
    }
  }

  void positionChanged() {
    _bitPosition = static_cast<uint64_t>(_position) * 8;
    if (_position > _length) {
      _length = _position;
    }
  }

  void expandCapacity(uint32_t length);
  void writeBit8(Bit8 data);
  void writeBit16(Bit16 data);
  void writeBit24(Bit32 data);
  void writeBit32(Bit32 data);
  void writeBit64(Bit64 data);

  ByteOrder _order = ByteOrder::LittleEndian;
  uint8_t* bytes = nullptr;
  uint32_t capacity = 0;
  uint32_t _length = 0;
  uint32_t _position = 0;
  uint64_t _bitPosition = 0;
};
}  // namespace pag
