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

#include "GeometryProcessor.h"

namespace tgfx {
/**
 * Returns the size of the attrib type in bytes.
 */
static constexpr size_t VertexAttribTypeSize(ShaderVar::Type type) {
  switch (type) {
    case ShaderVar::Type::Float:
      return sizeof(float);
    case ShaderVar::Type::Float2:
      return 2 * sizeof(float);
    case ShaderVar::Type::Float3:
      return 3 * sizeof(float);
    case ShaderVar::Type::Float4:
      return 4 * sizeof(float);
    default:
      return 0;
  }
}

template <typename T>
static constexpr T Align4(T x) {
  return (x + 3) >> 2 << 2;
}

size_t GeometryProcessor::Attribute::sizeAlign4() const {
  return Align4(VertexAttribTypeSize(_gpuType));
}

void GeometryProcessor::computeProcessorKey(Context*, BytesKey* bytesKey) const {
  bytesKey->write(classID());
  onComputeProcessorKey(bytesKey);
  for (const auto* attribute : attributes) {
    attribute->computeKey(bytesKey);
  }
}

void GeometryProcessor::setVertexAttributes(const Attribute* attrs, int attrCount) {
  for (int i = 0; i < attrCount; ++i) {
    if (attrs[i].isInitialized()) {
      attributes.push_back(attrs + i);
    }
  }
}
}  // namespace tgfx
