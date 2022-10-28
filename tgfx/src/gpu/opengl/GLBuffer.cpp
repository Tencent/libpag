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

#include "GLBuffer.h"

#include "GLContext.h"
#include "GLUtil.h"
#include "core/utils/UniqueID.h"

namespace tgfx {
static void ComputeRecycleKey(BytesKey* recycleKey, BufferType bufferType) {
  static const uint32_t Type = UniqueID::Next();
  recycleKey->write(Type);
  recycleKey->write(static_cast<uint32_t>(bufferType));
}

std::shared_ptr<GpuBuffer> GpuBuffer::Make(Context* context, BufferType bufferType,
                                           const void* buffer, size_t size) {
  if (buffer == nullptr || size == 0) {
    return nullptr;
  }
  auto target = bufferType == BufferType::Index ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
  BytesKey recycleKey = {};
  ComputeRecycleKey(&recycleKey, bufferType);
  auto glBuffer =
      std::static_pointer_cast<GLBuffer>(context->resourceCache()->getRecycled(recycleKey));
  auto gl = GLFunctions::Get(context);
  if (glBuffer == nullptr) {
    unsigned bufferID = 0;
    gl->genBuffers(1, &bufferID);
    if (bufferID == 0) {
      return nullptr;
    }
    glBuffer = Resource::Wrap(context, new GLBuffer(bufferType, size, bufferID));
  }
  gl->bindBuffer(target, glBuffer->_bufferID);
  gl->bufferData(target, static_cast<GLsizeiptr>(size), buffer, GL_STATIC_DRAW);
  if (!CheckGLError(context)) {
    gl->bindBuffer(target, 0);
    return nullptr;
  }
  gl->bindBuffer(target, 0);
  return glBuffer;
}

void GLBuffer::computeRecycleKey(BytesKey* bytesKey) const {
  ComputeRecycleKey(bytesKey, _bufferType);
}

void GLBuffer::onReleaseGPU() {
  if (_bufferID > 0) {
    auto gl = GLFunctions::Get(context);
    gl->deleteBuffers(1, &_bufferID);
    _bufferID = 0;
  }
}
}  // namespace tgfx
