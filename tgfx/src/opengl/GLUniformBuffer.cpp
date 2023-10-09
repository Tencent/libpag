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

#include "GLUniformBuffer.h"
#include "tgfx/opengl/GLFunctions.h"
#include "utils/Log.h"

namespace tgfx {
static size_t GetUniformSize(GLUniform::Type type) {
  switch (type) {
    case GLUniform::Type::Float:
    case GLUniform::Type::Int:
      return 4;
    case GLUniform::Type::Float2:
    case GLUniform::Type::Int2:
      return 8;
    case GLUniform::Type::Float3:
    case GLUniform::Type::Int3:
      return 12;
    case GLUniform::Type::Float4:
    case GLUniform::Type::Int4:
    case GLUniform::Type::Float2x2:
      return 16;
    case GLUniform::Type::Float3x3:
      return 36;
    case GLUniform::Type::Float4x4:
      return 64;
  }
  return 0;
}

GLUniformBuffer::GLUniformBuffer(const std::vector<GLUniform>& glUniforms) {
  size_t offset = 0;
  for (auto& glUniform : glUniforms) {
    auto size = GetUniformSize(glUniform.type);
    if (size > 0) {
      uniforms[glUniform.name] = {offset, glUniform.type, glUniform.location, true};
      offset += size;
    }
  }
  if (offset > 0) {
    buffer = new (std::nothrow) uint8_t[offset];
    if (buffer == nullptr) {
      uniforms = {};
    }
  }
}

GLUniformBuffer::~GLUniformBuffer() {
  delete[] buffer;
}

void GLUniformBuffer::setData(const std::string& name, const void* data) {
  auto key = getUniformKey(name);
  auto result = uniforms.find(key);
  if (result == uniforms.end()) {
    LOGE("GLUniformBuffer::setData: uniform %s not found", name.c_str());
    return;
  }
  auto& uniform = result->second;
  auto size = GetUniformSize(uniform.type);
  if (!uniform.dirty && memcmp(buffer + uniform.offset, data, size) == 0) {
    return;
  }
  uniform.dirty = true;
  bufferChanged = true;
  memcpy(buffer + uniform.offset, data, size);
}

void GLUniformBuffer::uploadToGPU(Context* context) {
  if (!bufferChanged) {
    return;
  }
  bufferChanged = false;
  auto gl = GLFunctions::Get(context);
  for (auto& item : uniforms) {
    auto& uniform = item.second;
    if (!uniform.dirty) {
      continue;
    }
    uniform.dirty = false;
    switch (uniform.type) {
      case GLUniform::Type::Float:
        gl->uniform1fv(uniform.location, 1, reinterpret_cast<float*>(buffer + uniform.offset));
        break;
      case GLUniform::Type::Float2:
        gl->uniform2fv(uniform.location, 1, reinterpret_cast<float*>(buffer + uniform.offset));
        break;
      case GLUniform::Type::Float3:
        gl->uniform3fv(uniform.location, 1, reinterpret_cast<float*>(buffer + uniform.offset));
        break;
      case GLUniform::Type::Float4:
        gl->uniform4fv(uniform.location, 1, reinterpret_cast<float*>(buffer + uniform.offset));
        break;
      case GLUniform::Type::Float2x2:
        gl->uniformMatrix2fv(uniform.location, 1, GL_FALSE,
                             reinterpret_cast<float*>(buffer + uniform.offset));
        break;
      case GLUniform::Type::Float3x3:
        gl->uniformMatrix3fv(uniform.location, 1, GL_FALSE,
                             reinterpret_cast<float*>(buffer + uniform.offset));
        break;
      case GLUniform::Type::Float4x4:
        gl->uniformMatrix4fv(uniform.location, 1, GL_FALSE,
                             reinterpret_cast<float*>(buffer + uniform.offset));
        break;
      case GLUniform::Type::Int:
        gl->uniform1iv(uniform.location, 1, reinterpret_cast<int*>(buffer + uniform.offset));
        break;
      case GLUniform::Type::Int2:
        gl->uniform2iv(uniform.location, 1, reinterpret_cast<int*>(buffer + uniform.offset));
        break;
      case GLUniform::Type::Int3:
        gl->uniform3iv(uniform.location, 1, reinterpret_cast<int*>(buffer + uniform.offset));
        break;
      case GLUniform::Type::Int4:
        gl->uniform4iv(uniform.location, 1, reinterpret_cast<int*>(buffer + uniform.offset));
        break;
    }
  }
}
}  // namespace tgfx
