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
static void ComputeRecycleKey(BytesKey* recycleKey, uint32_t type, size_t length) {
  static const uint32_t Type = UniqueID::Next();
  recycleKey->write(Type);
  recycleKey->write(type);
  recycleKey->write(static_cast<uint32_t>(length));
}

std::shared_ptr<GLBuffer> GLBuffer::Make(Context* context, const uint16_t* buffer, size_t length,
                                         uint32_t type) {
  if (buffer == nullptr || length == 0) {
    return nullptr;
  }
  BytesKey recycleKey = {};
  ComputeRecycleKey(&recycleKey, type, length);
  auto glBuffer =
      std::static_pointer_cast<GLBuffer>(context->resourceCache()->getRecycled(recycleKey));
  if (glBuffer != nullptr) {
    return glBuffer;
  }
  auto gl = GLFunctions::Get(context);
  unsigned bufferID = 0;
  gl->genBuffers(1, &bufferID);
  gl->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferID);
  gl->bufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(uint16_t) * length),
                 buffer, GL_STATIC_DRAW);
  if (!CheckGLError(context)) {
    gl->deleteBuffers(1, &bufferID);
    return nullptr;
  }
  gl->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
  return Resource::Wrap(context, new GLBuffer(type, length, bufferID));
}

void GLBuffer::computeRecycleKey(BytesKey* bytesKey) const {
  ComputeRecycleKey(bytesKey, type, _length);
}

void GLBuffer::onReleaseGPU() {
  if (_bufferID > 0) {
    auto gl = GLFunctions::Get(context);
    gl->deleteBuffers(1, &_bufferID);
    _bufferID = 0;
  }
}
}  // namespace tgfx
