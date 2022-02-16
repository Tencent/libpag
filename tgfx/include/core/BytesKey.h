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

#include <cstdint>
#include <vector>

namespace tgfx {
/**
 * A key used for hashing a byte stream.
 */
class BytesKey {
 public:
  /**
   * Returns true if this key is invalid.
   */
  bool isValid() const {
    return !values.empty();
  }

  /**
   * Writes a uint32 value into the key.
   */
  void write(uint32_t value);

  /**
   * Writes a pointer value into the key.
   */
  void write(const void* value);

  /**
   * Writes a uint32 value into the key.
   */
  void write(const uint8_t value[4]);

  /**
   * Writes a float value into the key.
   */
  void write(float value);

  friend bool operator==(const BytesKey& a, const BytesKey& b) {
    return a.values == b.values;
  }

  bool operator<(const BytesKey& key) const {
    return values < key.values;
  }

 private:
  std::vector<uint32_t> values = {};

  friend struct BytesHasher;
};

/**
 * The hasher for BytesKey.
 */
struct BytesHasher {
  size_t operator()(const BytesKey& key) const;
};
}  // namespace tgfx
