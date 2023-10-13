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

#include "gpu/StagedUniformBuffer.h"
#include "tgfx/gpu/Context.h"

namespace tgfx {
/**
 * Reflected description of a uniform variable in the GPU program.
 */
struct GLUniform {
  /**
   * Possible types of a uniform variable.
   */
  enum class Type {
    Float,
    Float2,
    Float3,
    Float4,
    Float2x2,
    Float3x3,
    Float4x4,
    Int,
    Int2,
    Int3,
    Int4,
  };

  std::string name;
  Type type;
  int location;
};

class GLUniformBuffer : public StagedUniformBuffer {
 public:
  explicit GLUniformBuffer(const std::vector<GLUniform>& uniforms);

  ~GLUniformBuffer() override;

  void uploadToGPU(Context* context);

 protected:
  void onCopyData(int index, const void* data, size_t dataSize) override;

 private:
  struct UniformBlock {
    size_t offset;
    GLUniform::Type type;
    int location;
    bool dirty;
  };

  uint8_t* buffer = nullptr;
  bool bufferChanged = false;
  std::vector<UniformBlock> uniforms = {};
};
}  // namespace tgfx
