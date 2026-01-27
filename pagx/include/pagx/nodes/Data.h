/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <cstddef>
#include <cstdint>
#include <memory>

namespace pagx {

/**
 * Data holds an immutable byte buffer.
 */
class Data {
 public:
  /**
   * Creates a Data object by copying the specified data.
   */
  static std::shared_ptr<Data> MakeWithCopy(const void* data, size_t length);

  ~Data();

  /**
   * Returns the memory address of the data.
   */
  const void* data() const {
    return _data;
  }

  /**
   * Returns the read-only memory address of the data cast to uint8_t*.
   */
  const uint8_t* bytes() const {
    return reinterpret_cast<const uint8_t*>(_data);
  }

  /**
   * Returns the byte size.
   */
  size_t size() const {
    return _size;
  }

  /**
   * Returns true if the Data is empty.
   */
  bool empty() const {
    return _size == 0;
  }

 private:
  Data(const void* data, size_t length);

  const void* _data = nullptr;
  size_t _size = 0;
};

}  // namespace pagx
