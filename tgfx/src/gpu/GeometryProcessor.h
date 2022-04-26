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

#include <vector>
#include "Processor.h"
#include "ShaderVar.h"
#include "tgfx/gpu/TextureSampler.h"

namespace tgfx {
class GLGeometryProcessor;

class GeometryProcessor : public Processor {
 public:
  /**
   * Describes a vertex attribute.
   */
  class Attribute {
   public:
    Attribute() = default;
    Attribute(std::string name, ShaderVar::Type gpuType)
        : _name(std::move(name)), _gpuType(gpuType) {
    }

    bool isInitialized() const {
      return !_name.empty();
    }

    const std::string& name() const {
      return _name;
    }
    ShaderVar::Type gpuType() const {
      return _gpuType;
    }

    size_t sizeAlign4() const;

    ShaderVar asShaderVar() const {
      return {_name, _gpuType, ShaderVar::TypeModifier::Attribute};
    }

    void computeKey(BytesKey* bytesKey) const {
      bytesKey->write(isInitialized() ? static_cast<uint32_t>(_gpuType) : ~0);
    }

   private:
    std::string _name;
    ShaderVar::Type _gpuType = ShaderVar::Type::Float;
  };

  const std::vector<const Attribute*>& vertexAttributes() const {
    return attributes;
  }

  void computeProcessorKey(Context* context, BytesKey* bytesKey) const override;

  virtual std::unique_ptr<GLGeometryProcessor> createGLInstance() const = 0;

 protected:
  void setVertexAttributes(const Attribute* attrs, int attrCount);

 private:
  virtual void onComputeProcessorKey(BytesKey* bytesKey) const = 0;

  std::vector<const Attribute*> attributes = {};
};
}  // namespace tgfx
