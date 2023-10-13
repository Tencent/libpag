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

#include "UniformBuffer.h"
#include "utils/Log.h"

namespace tgfx {
size_t Uniform::size() const {
  switch (type) {
    case Uniform::Type::Float:
    case Uniform::Type::Int:
      return 4;
    case Uniform::Type::Float2:
    case Uniform::Type::Int2:
      return 8;
    case Uniform::Type::Float3:
    case Uniform::Type::Int3:
      return 12;
    case Uniform::Type::Float4:
    case Uniform::Type::Int4:
    case Uniform::Type::Float2x2:
      return 16;
    case Uniform::Type::Float3x3:
      return 36;
    case Uniform::Type::Float4x4:
      return 64;
  }
  return 0;
}

UniformBuffer::UniformBuffer(std::vector<Uniform> uniformList) : uniforms(std::move(uniformList)) {
  int index = 0;
  size_t offset = 0;
  for (auto& uniform : uniforms) {
    uniformMap[uniform.name] = index++;
    offsets.push_back(offset);
    offset += uniform.size();
  }
}

void UniformBuffer::setData(const std::string& name, const tgfx::Matrix& matrix) {
  float values[6];
  matrix.get6(values);
  float data[] = {values[0], values[3], 0, values[1], values[4], 0, values[2], values[5], 1};
  onSetData(name, data, sizeof(data));
}

void UniformBuffer::onSetData(const std::string& name, const void* data, size_t size) {
  auto key = getUniformKey(name);
  auto result = uniformMap.find(key);
  if (result == uniformMap.end()) {
    LOGE("UniformBuffer::onSetData() uniform '%s' not found!", name.c_str());
    return;
  }
  auto index = result->second;
  auto uniformSize = uniforms[index].size();
  if (uniformSize != size) {
    LOGE("UniformBuffer::onSetData() data size mismatch!");
    return;
  }
  onCopyData(index, offsets[index], size, data);
}

}  // namespace tgfx