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
class GLUniformBuffer : public StagedUniformBuffer {
 public:
  GLUniformBuffer(std::vector<Uniform> uniforms, std::vector<int> locations);

  ~GLUniformBuffer() override;

  void uploadToGPU(Context* context);

 protected:
  void onCopyData(int index, size_t offset, size_t size, const void* data) override;

 private:
  uint8_t* buffer = nullptr;
  bool bufferChanged = false;
  std::vector<int> locations = {};
  std::vector<bool> dirtyFlags = {};
};
}  // namespace tgfx
