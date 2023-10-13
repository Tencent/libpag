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

#include "gpu/UniformBuffer.h"
#include "tgfx/gpu/Context.h"

namespace tgfx {
/**
 * StagedUniformBuffer is a UniformBuffer that supports multiple stages.
 */
class StagedUniformBuffer : public UniformBuffer {
 public:
  /**
   * Returns the mangled name for the specified uniform name and stage index.
   */
  static std::string GetMangledName(const std::string& name, int stageIndex);

  /**
   * advanceStage is called by Program between each processor's set data. It increments the stage
   * offset for uniform name mangling. If advanceStage is not called, the uniform names will not be
   * mangled.
   */
  void advanceStage() {
    stageIndex++;
  }

  /**
   * Resets the stage offset and uploads the uniform data to the GPU.
   */
  void resetStage() {
    stageIndex = -1;
  }

 protected:
  explicit StagedUniformBuffer(const std::vector<std::pair<std::string, size_t>>& uniforms);

  /**
   * Generates a uniform key based on the specified name and the current stage index.
   */
  std::string getUniformKey(const std::string& name) const override;

 private:
  int stageIndex = -1;
};
}  // namespace tgfx
