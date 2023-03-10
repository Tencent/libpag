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

#include "GLProgramDataManager.h"
#include "GLUniformHandler.h"
#include "GLUtil.h"

namespace tgfx {
GLProgramDataManager::GLProgramDataManager(Context* context, const std::vector<int>* uniforms)
    : gl(GLFunctions::Get(context)), uniforms(uniforms) {
}

void GLProgramDataManager::set1f(UniformHandle handle, float v0) const {
  auto location = uniforms->at(handle.toIndex());
  if (kUnusedUniform != location) {
    gl->uniform1f(location, v0);
  }
}

void GLProgramDataManager::set2f(UniformHandle handle, float v0, float v1) const {
  auto location = uniforms->at(handle.toIndex());
  if (kUnusedUniform != location) {
    gl->uniform2f(location, v0, v1);
  }
}

void GLProgramDataManager::set4f(UniformHandle handle, float v0, float v1, float v2,
                                 float v3) const {
  auto location = uniforms->at(handle.toIndex());
  if (kUnusedUniform != location) {
    gl->uniform4f(location, v0, v1, v2, v3);
  }
}

void GLProgramDataManager::set4fv(UniformHandle handle, int arrayCount, const float* v) const {
  auto location = uniforms->at(handle.toIndex());
  if (kUnusedUniform != location) {
    gl->uniform4fv(location, arrayCount, v);
  }
}

void GLProgramDataManager::setMatrix3f(UniformHandle handle, const float matrix[]) const {
  auto location = uniforms->at(handle.toIndex());
  if (kUnusedUniform != location) {
    gl->uniformMatrix3fv(location, 1, GL_FALSE, matrix);
  }
}

void GLProgramDataManager::setMatrix4f(UniformHandle handle, const float matrix[]) const {
  auto location = uniforms->at(handle.toIndex());
  if (kUnusedUniform != location) {
    gl->uniformMatrix4fv(location, 1, GL_FALSE, matrix);
  }
}

void GLProgramDataManager::setMatrix(UniformHandle u, const Matrix& matrix) const {
  auto values = ToGLMatrix(matrix);
  setMatrix3f(u, &(values[0]));
}
}  // namespace tgfx
