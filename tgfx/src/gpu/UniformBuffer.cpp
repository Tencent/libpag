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
UniformBuffer::UniformBuffer(const std::vector<std::pair<std::string, size_t>>& uniforms) {
  int index = 0;
  for (auto& uniform : uniforms) {
    handles[uniform.first] = {index++, uniform.second};
  }
}

void UniformBuffer::setData(const std::string& name, const tgfx::Matrix& matrix) {
  float values[6];
  matrix.get6(values);
  float data[] = {values[0], values[3], 0, values[1], values[4], 0, values[2], values[5], 1};
  onSetData(name, data, sizeof(data));
}

void UniformBuffer::onSetData(const std::string& name, const void* data, size_t dataSize) {
  auto key = getUniformKey(name);
  auto result = handles.find(key);
  if (result == handles.end()) {
    LOGE("UniformBuffer::onSetData() uniform '%s' not found!", name.c_str());
    return;
  }
  auto& handle = result->second;
  if (handle.size != dataSize) {
    LOGE("UniformBuffer::onSetData() data size mismatch!");
    return;
  }
  onCopyData(handle.index, data, handle.size);
}

}  // namespace tgfx