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

#include "FragmentProcessor.h"
#include "FragmentShaderBuilder.h"
#include "GeometryProcessor.h"
#include "ProgramDataManager.h"
#include "UniformHandler.h"
#include "VaryingHandler.h"
#include "VertexShaderBuilder.h"

namespace pag {
class GLGeometryProcessor {
 public:
  // Use only for easy-to-use aliases.
  using FPCoordTransformIter = FragmentProcessor::CoordTransformIter;

  virtual ~GLGeometryProcessor() = default;

  /**
   * This class provides access to the CoordTransforms across all FragmentProcessors in a
   * Pipeline. It is also used by the primitive processor to specify the fragment shader
   * variable that will hold the transformed coords for each CoordTransform. It is required that
   * the primitive processor iterate over each coord transform and insert a shader var result for
   * each. The GLFragmentProcessors will reference these variables in their fragment code.
   */
  class FPCoordTransformHandler {
   public:
    FPCoordTransformHandler(const Pipeline& pipeline, std::vector<ShaderVar>* transformedCoordVars)
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
             const GeometryProcessor* gp, std::string outputColor, std::string outputCoverage,
             FPCoordTransformHandler* transformHandler)
        : vertBuilder(vertBuilder),
          fragBuilder(fragBuilder),
          varyingHandler(varyingHandler),
          uniformHandler(uniformHandler),
          caps(caps),
          gp(gp),
          outputColor(std::move(outputColor)),
          outputCoverage(std::move(outputCoverage)),
          fpCoordTransformHandler(transformHandler) {
    }
    VertexShaderBuilder* vertBuilder;
    FragmentShaderBuilder* fragBuilder;
    VaryingHandler* varyingHandler;
    UniformHandler* uniformHandler;
    const Caps* caps;
    const GeometryProcessor* gp;
    const std::string outputColor;
    const std::string outputCoverage;
    FPCoordTransformHandler* fpCoordTransformHandler;
  };

  virtual void emitCode(EmitArgs&) = 0;

  /**
   * A GLGeometryProcessor instance can be reused with any GeometryProcessor that
   * produces the same stage key; this function reads data from a GeometryProcessor and
   * uploads any uniform variables required  by the shaders created in emitCode(). The
   * GeometryProcessor parameter is guaranteed to be of the same type and to have an
   * identical processor key as the GeometryProcessor that created this GLGeometryProcessor.
   */
  virtual void setData(const ProgramDataManager& programDataManager,
                       const GeometryProcessor& geometryProcessor,
                       FPCoordTransformIter* coordTransformIter) = 0;

 protected:
  /**
   * A helper to upload coord transform matrices in setData().
   */
  void setTransformDataHelper(const Matrix& localMatrix,
                              const ProgramDataManager& programDataManager,
                              FPCoordTransformIter* transformIter);

  /**
   * Emit transformed local coords from the vertex shader as a uniform matrix and varying per
   * coord-transform. localCoordsVar must be a 2-component vector.
   */
  void emitTransforms(VertexShaderBuilder* vertexBuilder, VaryingHandler* varyingHandler,
                      UniformHandler* uniformHandler, const ShaderVar& localCoordsVar,
                      FPCoordTransformHandler* transformHandler);

 private:
  struct TransformUniform {
    UniformHandle handle = {};
    Matrix currentMatrix = Matrix::I();
    bool updated = false;
  };

  std::vector<TransformUniform> installedTransforms;
};
}  // namespace pag
