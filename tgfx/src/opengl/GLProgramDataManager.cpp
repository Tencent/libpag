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
GLProgramDataManager::GLProgramDataManager(Context* context,
                                           std::unordered_map<std::string, int>* uniforms)
    : gl(GLFunctions::Get(context)), uniforms(uniforms) {
}

void GLProgramDataManager::set1f(const std::string& name, float v0) const {
  auto location = getUniformLocation(name);
  if (UNUSED_UNIFORM != location) {
    gl->uniform1f(location, v0);
  }
}

void GLProgramDataManager::set2f(const std::string& name, float v0, float v1) const {
  auto location = getUniformLocation(name);
  if (UNUSED_UNIFORM != location) {
    gl->uniform2f(location, v0, v1);
  }
}

void GLProgramDataManager::set4f(const std::string& name, float v0, float v1, float v2,
                                 float v3) const {
  auto location = getUniformLocation(name);
  if (UNUSED_UNIFORM != location) {
    gl->uniform4f(location, v0, v1, v2, v3);
  }
}

void GLProgramDataManager::set4fv(const std::string& name, int arrayCount, const float* v) const {
  auto location = getUniformLocation(name);
  if (UNUSED_UNIFORM != location) {
    gl->uniform4fv(location, arrayCount, v);
  }
}

void GLProgramDataManager::setMatrix3f(const std::string& name, const float matrix[]) const {
  auto location = getUniformLocation(name);
  if (UNUSED_UNIFORM != location) {
    gl->uniformMatrix3fv(location, 1, GL_FALSE, matrix);
  }
}

void GLProgramDataManager::setMatrix4f(const std::string& name, const float matrix[]) const {
  auto location = getUniformLocation(name);
  if (UNUSED_UNIFORM != location) {
    gl->uniformMatrix4fv(location, 1, GL_FALSE, matrix);
  }
}

void GLProgramDataManager::setMatrix(const std::string& name, const Matrix& matrix) const {
  auto values = ToGLMatrix(matrix);
  setMatrix3f(name, &(values[0]));
}

int GLProgramDataManager::getUniformLocation(const std::string& name) const {
  auto key = getUniformKey(name);
  auto result = uniforms->find(key);
  if (result == uniforms->end()) {
    return UNUSED_UNIFORM;
  }
  return result->second;
}
}  // namespace tgfx
