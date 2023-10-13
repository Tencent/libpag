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

#include <string>
#include <unordered_map>
#include <vector>
#include "tgfx/core/Matrix.h"

namespace tgfx {
/**
 * An object representing the collection of uniform variables in a GPU program.
 */
class UniformBuffer {
 public:
  virtual ~UniformBuffer() = default;

  /**
   * Copies value into the uniform buffer. The data must have the same size as the uniform specified
   * by name.
   */
  template <typename T>
  std::enable_if_t<std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>, void> setData(
      const std::string& name, const T& value) {
    onSetData(name, &value, sizeof(value));
  }

  /**
   * Convenience method for copying a Matrix to a 3x3 matrix in column-major order.
   */
  void setData(const std::string& name, const Matrix& matrix);

 protected:
  /**
   * Constructs a uniform buffer with the specified names and sizes.
   */
  explicit UniformBuffer(const std::vector<std::pair<std::string, size_t>>& uniforms);

  /**
   * Generates a uniform key based on the specified name and the current stage index.
   */
  virtual std::string getUniformKey(const std::string& name) const {
    return name;
  }

  /**
   * Copies data into the uniform buffer. The data must have the same size as the uniform specified
   * by name.
   */
  virtual void onCopyData(int index, const void* data, size_t dataSize) = 0;

 private:
  struct Handle {
    int index = -1;
    size_t size = 0;
  };

  std::unordered_map<std::string, Handle> handles = {};

  void onSetData(const std::string& name, const void* data, size_t dataSize);
};
}  // namespace tgfx
