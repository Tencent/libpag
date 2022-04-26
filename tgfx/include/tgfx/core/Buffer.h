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

#include <memory>
#include "tgfx/core/Data.h"

namespace tgfx {
/**
 * An object representing a mutable byte buffer in memory.
 */
class Buffer {
 public:
  /**
   * Creates an empty Buffer.
   */
  Buffer() = default;

  /**
   * Creates a new Buffer with the specified length. The contents are uninitialized.
   */
  explicit Buffer(size_t length);

  /**
   * Creates a new Buffer by copying the specified data.
   */
  Buffer(const void* data, size_t length);

  /**
   * Creates a new Buffer by copying the specified Data object.
   */
  explicit Buffer(std::shared_ptr<Data> data);

  ~Buffer();

  /**
   * Returns the memory address of the data.
   */
  void* data() const {
    return _data;
  }

  /**
   * Returns the memory address of the data, but in this case it is cast to uint8_t*, to make it
   * easy to add an offset to it.
   */
  uint8_t* bytes() const {
    return _data;
  }

  /**
   * Returns the byte size.
   */
  size_t size() const {
    return _size;
  }

  /**
   * Returns true if the Buffer is empty.
   */
  bool empty() const {
    return _size == 0;
  }

  /**
   * Sets all bytes of the buffer to zero.
   */
  void clear();

  /**
   * Copies a range of the buffer bytes into a Data object. If offset or length is out of range,
   * they are clamped to the beginning and end of the buffer. Returns nullptr if the clamped length
   * is 0.
   */
  std::shared_ptr<Data> copyRange(size_t offset, size_t length);

  /**
   * Writes a byte array into the specified range of the buffer. If offset or length is out of
   * range, they are clamped to the beginning and end of the buffer. The buffer will be unchanged if
   * the clamped length is 0.
   */
  void writeRange(size_t offset, size_t length, const void* bytes);

  /**
   * Writes a Data object into the specified offset of the buffer. If offset or data size is out of
   * range, they are clamped to the beginning and end of the buffer. The buffer will be unchanged if
   * the clamped data size is 0.
   */
  void writeRange(size_t offset, std::shared_ptr<Data> data) {
    writeRange(offset, data->size(), data->data());
  }

  /**
   * Transfers the ownership of the managed bytes to the returned Data object, and set this Buffer
   * to empty. Returns nullptr if the Buffer is already empty.
   */
  std::shared_ptr<Data> release();

  /**
   * Returns one byte.
   */
  uint8_t operator[](size_t index) const;

  /**
   * Returns the reference of one byte.
   */
  uint8_t& operator[](size_t index);

 private:
  uint8_t* _data = nullptr;
  size_t _size = 0;

  Buffer(Buffer&) = delete;
  Buffer& operator=(Buffer&) = delete;
  size_t getClampedLength(size_t offset, size_t length) const;
};
}  // namespace tgfx
