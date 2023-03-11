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

#include "GLContext.h"
#include "gpu/ProgramDataManager.h"

namespace tgfx {
class GLProgramDataManager : public ProgramDataManager {
 public:
  GLProgramDataManager(Context* context, const std::vector<int>* uniforms);

  void set1f(UniformHandle handle, float v0) const override;

  void set2f(UniformHandle handle, float v0, float v1) const override;

  void set4f(UniformHandle handle, float v0, float v1, float v2, float v3) const override;

  void set4fv(UniformHandle handle, int arrayCount, const float* v) const override;

  void setMatrix3f(UniformHandle handle, const float matrix[]) const override;

  void setMatrix4f(UniformHandle handle, const float matrix[]) const override;

  void setMatrix(UniformHandle u, const Matrix& matrix) const override;

 private:
  const GLFunctions* gl;
  const std::vector<int>* uniforms;
};
}  // namespace tgfx
