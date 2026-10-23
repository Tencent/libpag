/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  or required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace pagx {

/**
 * Data holds an immutable byte buffer. Slice variants reference a parent buffer and keep it
 * alive; the parent stays resident for the slice's whole lifetime (the v4 decoder relies on
 * this for zero-copy image references, spec §5.1).
 */
class Data {
 public:
  static std::shared_ptr<Data> MakeWithCopy(const void* data, size_t length);

  static std::shared_ptr<Data> MakeAdopt(uint8_t* data, size_t length);

  /**
   * Creates a Data slice referencing [offset, offset+length) inside the parent buffer. The
   * parent buffer stays alive for the slice's whole lifetime. Returns nullptr when the
   * range is out of bounds.
   */
  static std::shared_ptr<Data> MakeSlice(std::shared_ptr<Data> parent, size_t offset,
                                         size_t length);

  ~Data();

  Data(const Data&) = delete;
  Data& operator=(const Data&) = delete;
  Data(Data&&) = delete;
  Data& operator=(Data&&) = delete;

  const void* data() const {
    return _data;
  }

  const uint8_t* bytes() const {
    return _data;
  }

  size_t size() const {
    return _size;
  }

  bool empty() const {
    return _size == 0;
  }

 private:
  Data(const void* data, size_t length);
  Data(uint8_t* data, size_t length, bool adopt);
  Data(std::shared_ptr<Data> parent, size_t offset, size_t length);

  // Non-null for slices; keeps the parent buffer alive and marks the destructor not to free
  // the borrowed pointer.
  std::shared_ptr<Data> _parent = {};
  const uint8_t* _data = nullptr;
  size_t _size = 0;
};

}  // namespace pagx
