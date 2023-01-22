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

#include "tgfx/gpu/Resource.h"

namespace tgfx {
enum class BufferType {
  Index,
  Vertex,
};

class GpuBuffer : public Resource {
 public:
  static std::shared_ptr<GpuBuffer> Make(Context* context, BufferType bufferType,
                                         const void* buffer, size_t size);

  size_t size() const {
    return _sizeInBytes;
  }

  size_t memoryUsage() const override {
    return _sizeInBytes;
  }

 protected:
  GpuBuffer(BufferType bufferType, size_t sizeInBytes)
      : _bufferType(bufferType), _sizeInBytes(sizeInBytes) {
  }

  BufferType _bufferType;

 private:
  size_t _sizeInBytes;
};
}  // namespace tgfx
