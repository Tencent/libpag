/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <cinttypes>
#include <cmath>

namespace tgfx {
/**
 * ByteOrder describe how computers organize the bytes that make up numbers.
 */
enum class ByteOrder {
  /**
   * Stores bytes in order of least-to-most-significant, where the least significant byte takes the
   * first or lowest address.
   */
  LittleEndian,

  /**
   * The opposite order to 'LittleEndian'.
   */
  BigEndian
};

/**
 * The DataView view provides a low-level interface for reading and writing multiple number types in
 * a byte buffer, without having to care about the platform's endianness. DataView does not try to
 * manage the lifetime of the byte buffer. The caller must ensure that the byte buffer is valid for
 * the lifetime of the DataView.
 */
class DataView {
 public:
  /**
   * Creates an empty DataView without a byte buffer.
   */
  DataView() = default;

  /**
   * Creates a new DataView with a read-only byte buffer.
   */
  DataView(const uint8_t* buffer, size_t length, ByteOrder byteOrder = ByteOrder::LittleEndian);

  /**
   * Creates a new DataView with a writable byte buffer.
   */
  DataView(uint8_t* buffer, size_t length, ByteOrder byteOrder = ByteOrder::LittleEndian);

  /**
   * Sets the DataView to empty, bytes to nullptr, and leave the byte order unchanged.
   */
  void reset();

  /**
   * Sets the DataView to a read-only byte buffer, and leave the byte order unchanged.
   */
  void reset(const uint8_t* bytes, size_t length);

  /**
   * Set the DataView to a writable byte buffer, and leave the byte order unchanged.
   */
  void reset(uint8_t* bytes, size_t length);

  /**
   * Returns true if the Buffer is empty.
   */
  bool isEmpty() const {
    return _size == 0;
  }

  /**
   * Returns the pixel address, the base address corresponding to the pixel origin.
   */
  const uint8_t* bytes() const {
    return _bytes;
  }

  /**
   * Returns the writable pixel address, the base address corresponding to the pixel origin. Returns
   * nullptr if the Pixmap is constructed from read-only pixels.
   */
  void* writableBytes() const {
    return _writableBytes;
  }

  /**
   * Returns the byte size.
   */
  size_t size() const {
    return _size;
  }

  /**
   * Returns the DataView's byte order. The byte order is used when reading or writing multibyte
   * values. The default value is LittleEndian.
   */
  ByteOrder byteOrder() const {
    return _byteOrder;
  }

  /**
   * Sets the DataView's byte order.
   */
  void setByteOrder(ByteOrder order) {
    _byteOrder = order;
  }

  template <typename T>
  T read(size_t offset) const;

  /**
   * Reads a boolean value from the bytes at the specified byte offset. A signed 8-bit integer is
   * read, and true is returned if the integer is nonzero, false otherwise.
   */
  template <>
  bool read<bool>(size_t offset) const {
    return readBoolean(offset);
  }

  /**
   * Reads a signed 8-bit integer from the bytes at the specified byte offset.
   */
  template <>
  int8_t read<int8_t>(size_t offset) const {
    return readInt8(offset);
  }

  /**
   * Reads an unsigned 8-bit integer from the bytes at the specified byte offset.
   */
  template <>
  uint8_t read<uint8_t>(size_t offset) const {
    return readUint8(offset);
  }

  /**
   * Reads a signed 16-bit integer from the bytes at the specified byte offset.
   */
  template <>
  int16_t read<int16_t>(size_t offset) const {
    return readInt16(offset);
  }

  /**
   * Reads an unsigned 16-bit integer from the bytes at the specified byte offset.
   */
  template <>
  uint16_t read<uint16_t>(size_t offset) const {
    return readUint16(offset);
  }

  /**
   * Reads a signed 32-bit integer from the bytes at the specified byte offset.
   */
  template <>
  int32_t read<int32_t>(size_t offset) const {
    return readInt32(offset);
  }

  /**
   * Reads an unsigned 32-bit integer from the bytes at the specified byte offset.
   */
  template <>
  uint32_t read<uint32_t>(size_t offset) const {
    return readUint32(offset);
  }

  /**
   * Reads a signed 64-bit integer from the bytes at the specified byte offset.
   */
  template <>
  int64_t read<int64_t>(size_t offset) const {
    return readInt64(offset);
  }

  /**
   * Reads an unsigned 64-bit integer from the bytes at the specified byte offset.
   */
  template <>
  uint64_t read<uint64_t>(size_t offset) const {
    return readUint64(offset);
  }

  /**
   * Reads an IEEE 754 single-precision (32-bit) floating-point number from the bytes at the
   * specified byte offset.
   */
  template <>
  float read<float>(size_t offset) const {
    return readFloat(offset);
  }

  /**
   * Reads an IEEE 754 double-precision (64-bit) floating-point number from the bytes at the
   * specified byte offset.
   */
  template <>
  double read<double>(size_t offset) const {
    return readDouble(offset);
  }

  template <typename T>
  bool write(size_t offset, T value);

  /**
   * Writes a boolean value to the bytes at the specified byte offset. A signed 8-bit integer is
   * written according to the value parameter, either 1 if true or 0 if false. Returns false if the
   * offset is out of range or the DataView is constructed from read-only bytes.
   */
  template <>
  bool write<bool>(size_t offset, bool value) {
    return writeBoolean(offset, value);
  }

  /**
   * Writes an 8-bit signed integer to the bytes at the specified byte offset. Returns false if the
   * offset is out of range or the DataView is constructed from read-only bytes.
   */
  template <>
  bool write<int8_t>(size_t offset, int8_t value) {
    return writeInt8(offset, value);
  }

  /**
   * Writes an unsigned 8-bit integer to the bytes at the specified byte offset. Returns false if
   * the offset is out of range or the DataView is constructed from read-only bytes.
   */
  template <>
  bool write<uint8_t>(size_t offset, uint8_t value) {
    return writeUint8(offset, value);
  }

  /**
   * Writes a 16-bit signed integer to the bytes at the specified byte offset. Returns false if the
   * offset is out of range or the DataView is constructed from read-only bytes.
   */
  template <>
  bool write<int16_t>(size_t offset, int16_t value) {
    return writeInt16(offset, value);
  }

  /**
   * Writes an unsigned 16-bit integer to the bytes at the specified byte offset. Returns false if
   * the offset is out of range or the DataView is constructed from read-only bytes.
   */
  template <>
  bool write<uint16_t>(size_t offset, uint16_t value) {
    return writeUint16(offset, value);
  }

  /**
   * Writes a 32-bit signed integer to the bytes at the specified byte offset. Returns false if the
   * offset is out of range or the DataView is constructed from read-only bytes.
   */
  template <>
  bool write<int32_t>(size_t offset, int32_t value) {
    return writeInt32(offset, value);
  }

  /**
   * Writes an unsigned 32-bit integer to the bytes at the specified byte offset. Returns false if
   * the offset is out of range or the DataView is constructed from read-only bytes.
   */
  template <>
  bool write<uint32_t>(size_t offset, uint32_t value) {
    return writeUint32(offset, value);
  }

  /**
   * Writes a 64-bit signed integer to the bytes at the specified byte offset. Returns false if the
   * offset is out of range or the DataView is constructed from read-only bytes.
   */
  template <>
  bool write<int64_t>(size_t offset, int64_t value) {
    return writeInt64(offset, value);
  }

  /**
   * Writes an unsigned 64-bit integer to the bytes at the specified byte offset. Returns false if
   * the offset is out of range or the DataView is constructed from read-only bytes.
   */
  template <>
  bool write<uint64_t>(size_t offset, uint64_t value) {
    return writeUint64(offset, value);
  }

  /**
   * Writes an IEEE 754 single-precision (32-bit) floating-point number to the bytes at the
   * specified byte offset. Returns false if the offset is out of range or the DataView is
   * constructed from read-only bytes.
   */
  template <>
  bool write<float>(size_t offset, float value) {
    return writeFloat(offset, value);
  }

  /**
   * Writes an IEEE 754 double-precision (64-bit) floating-point number to the bytes at the
   * specified byte offset. Returns false if the offset is out of range or the DataView is
   * constructed from read-only bytes.
   */
  template <>
  bool write<double>(size_t offset, double value) {
    return writeDouble(offset, value);
  }

 private:
  const uint8_t* _bytes = nullptr;
  uint8_t* _writableBytes = nullptr;
  size_t _size = 0;
  ByteOrder _byteOrder = ByteOrder::LittleEndian;

  bool readBoolean(size_t offset) const;
  int8_t readInt8(size_t offset) const;
  uint8_t readUint8(size_t offset) const;
  int16_t readInt16(size_t offset) const;
  uint16_t readUint16(size_t offset) const;
  int32_t readInt32(size_t offset) const;
  uint32_t readUint32(size_t offset) const;
  int64_t readInt64(size_t offset) const;
  uint64_t readUint64(size_t offset) const;
  float readFloat(size_t offset) const;
  double readDouble(size_t offset) const;

  bool writeBoolean(size_t offset, bool value);
  bool writeInt8(size_t offset, int8_t value);
  bool writeUint8(size_t offset, uint8_t value);
  bool writeInt16(size_t offset, int16_t value);
  bool writeUint16(size_t offset, uint16_t value);
  bool writeInt32(size_t offset, int32_t value);
  bool writeUint32(size_t offset, uint32_t value);
  bool writeInt64(size_t offset, int64_t value);
  bool writeUint64(size_t offset, uint64_t value);
  bool writeFloat(size_t offset, float value);
  bool writeDouble(size_t offset, double value);

  bool readData(size_t offset, uint8_t* data, size_t byteSize) const;
  bool writeData(size_t offset, const uint8_t* data, size_t byteSize);
};
}  // namespace tgfx
