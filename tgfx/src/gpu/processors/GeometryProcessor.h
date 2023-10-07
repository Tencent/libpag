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
#include "gpu/FragmentShaderBuilder.h"
#include "gpu/ShaderVar.h"
#include "gpu/TextureSampler.h"
#include "gpu/UniformBuffer.h"
#include "gpu/UniformHandler.h"
#include "gpu/VaryingHandler.h"
#include "gpu/VertexShaderBuilder.h"
#include "gpu/processors/FragmentProcessor.h"
#include "gpu/processors/Processor.h"

namespace tgfx {
class GeometryProcessor : public Processor {
 public:
  // Use only for easy-to-use aliases.
  using FPCoordTransformIter = FragmentProcessor::CoordTransformIter;

  /**
   * Describes a vertex attribute.
   */
  class Attribute {
   public:
    Attribute() = default;
    Attribute(std::string name, SLType gpuType) : _name(std::move(name)), _gpuType(gpuType) {
    }

    bool isInitialized() const {
      return !_name.empty();
    }

    const std::string& name() const {
      return _name;
    }
    SLType gpuType() const {
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
    SLType _gpuType = SLType::Float;
  };

  const std::vector<const Attribute*>& vertexAttributes() const {
    return attributes;
  }

  void computeProcessorKey(Context* context, BytesKey* bytesKey) const override;

  class FPCoordTransformHandler {
   public:
    FPCoordTransformHandler(const Pipeline* pipeline, std::vector<ShaderVar>* transformedCoordVars)
        : iter(pipeline), transformedCoordVars(transformedCoordVars) {
    }

    const CoordTransform* nextCoordTransform() {
      return iter.next();
    }

    // 'args' are constructor params to ShaderVar.
    template <typename... Args>
    void specifyCoordsForCurrCoordTransform(Args&&... args) {
      transformedCoordVars->emplace_back(std::forward<Args>(args)...);
    }

   private:
    FragmentProcessor::CoordTransformIter iter;
    std::vector<ShaderVar>* transformedCoordVars;
  };

  struct EmitArgs {
    EmitArgs(VertexShaderBuilder* vertBuilder, FragmentShaderBuilder* fragBuilder,
             VaryingHandler* varyingHandler, UniformHandler* uniformHandler, const Caps* caps,
             std::string outputColor, std::string outputCoverage,
             FPCoordTransformHandler* transformHandler)
        : vertBuilder(vertBuilder),
          fragBuilder(fragBuilder),
          varyingHandler(varyingHandler),
          uniformHandler(uniformHandler),
          caps(caps),
          outputColor(std::move(outputColor)),
          outputCoverage(std::move(outputCoverage)),
          fpCoordTransformHandler(transformHandler) {
    }
    VertexShaderBuilder* vertBuilder;
    FragmentShaderBuilder* fragBuilder;
    VaryingHandler* varyingHandler;
    UniformHandler* uniformHandler;
    const Caps* caps;
    const std::string outputColor;
    const std::string outputCoverage;
    FPCoordTransformHandler* fpCoordTransformHandler;
  };

  virtual void emitCode(EmitArgs&) const = 0;

  virtual void setData(UniformBuffer* uniformBuffer,
                       FPCoordTransformIter* coordTransformIter) const = 0;

 protected:
  explicit GeometryProcessor(uint32_t classID) : Processor(classID) {
  }

  void setVertexAttributes(const Attribute* attrs, int attrCount);

  /**
   * A helper to upload coord transform matrices in setData().
   */
  void setTransformDataHelper(const Matrix& localMatrix, UniformBuffer* uniformBuffer,
                              FPCoordTransformIter* transformIter) const;

  /**
   * Emit transformed local coords from the vertex shader as a uniform matrix and varying per
   * coord-transform. localCoordsVar must be a 2-component vector.
   */
  void emitTransforms(VertexShaderBuilder* vertexBuilder, VaryingHandler* varyingHandler,
                      UniformHandler* uniformHandler, const ShaderVar& localCoordsVar,
                      FPCoordTransformHandler* transformHandler) const;

 private:
  virtual void onComputeProcessorKey(BytesKey*) const {
  }

  std::vector<const Attribute*> attributes = {};
};
}  // namespace tgfx
