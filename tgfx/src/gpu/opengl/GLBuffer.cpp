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

namespace pag {
static void ComputeRecycleKey(BytesKey* recycleKey, const void* uniqueKey, size_t length) {
  static const uint32_t Type = UniqueID::Next();
  recycleKey->write(Type);
  if (uniqueKey) {
    recycleKey->write(uniqueKey);
    recycleKey->write(static_cast<uint32_t>(length));
  }
}

std::shared_ptr<GLBuffer> GLBuffer::Make(Context* context, const uint16_t* buffer, size_t length) {
  BytesKey recycleKey = {};
  ComputeRecycleKey(&recycleKey, buffer, length);
  auto glBuffer = std::static_pointer_cast<GLBuffer>(context->getRecycledResource(recycleKey));
  if (glBuffer != nullptr) {
    return glBuffer;
  }
  const auto* gl = GLContext::Unwrap(context);
  glBuffer = Resource::Wrap(context, new GLBuffer());
  glBuffer->uniqueKey = buffer;
  glBuffer->_length = length;
  gl->genBuffers(1, &glBuffer->_bufferID);
  if (buffer) {
    gl->bindBuffer(GL::ELEMENT_ARRAY_BUFFER, glBuffer->_bufferID);
    gl->bufferData(GL::ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(uint16_t) * length),
                   buffer, GL::STATIC_DRAW);
    gl->bindBuffer(GL::ELEMENT_ARRAY_BUFFER, 0);
  }
  return glBuffer;
}

void GLBuffer::computeRecycleKey(BytesKey* bytesKey) const {
  ComputeRecycleKey(bytesKey, uniqueKey, _length);
}

void GLBuffer::onRelease(Context* context) {
  if (_bufferID > 0) {
    GLContext::Unwrap(context)->deleteBuffers(1, &_bufferID);
    _bufferID = 0;
  }
}
}  // namespace pag
