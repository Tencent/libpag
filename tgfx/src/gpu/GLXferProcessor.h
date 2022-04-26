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

#include "gpu/FragmentShaderBuilder.h"
#include "gpu/ProgramDataManager.h"
#include "gpu/UniformHandler.h"
#include "gpu/XferProcessor.h"
#include "tgfx/gpu/Texture.h"

namespace tgfx {
class GLXferProcessor {
 public:
  virtual ~GLXferProcessor() = default;

  struct EmitArgs {
    EmitArgs(FragmentShaderBuilder* fragBuilder, UniformHandler* uniformHandler,
             const XferProcessor* xferProcessor, std::string inputColor, std::string inputCoverage,
             std::string outputColor, const SamplerHandle dstTextureSamplerHandle)
        : fragBuilder(fragBuilder),
          uniformHandler(uniformHandler),
          xferProcessor(xferProcessor),
          inputColor(std::move(inputColor)),
          inputCoverage(std::move(inputCoverage)),
          outputColor(std::move(outputColor)),
          dstTextureSamplerHandle(dstTextureSamplerHandle) {
    }
    FragmentShaderBuilder* fragBuilder;
    UniformHandler* uniformHandler;
    const XferProcessor* xferProcessor;
    const std::string inputColor;
    const std::string inputCoverage;
    const std::string outputColor;
    const SamplerHandle dstTextureSamplerHandle;
  };

  virtual void emitCode(const EmitArgs& args) = 0;

  /**
   * A GLXferProcessor instance can be reused with any XferProcessor that produces
   * the same stage key; this function reads data from a XferProcessor and uploads any
   * uniform variables required by the shaders created in emitCode(). The XferProcessor
   * parameter is guaranteed to be of the same type that created this GLXferProcessor and
   * to have an identical processor key as the one that created this GLXferProcessor.
   */
  virtual void setData(const ProgramDataManager&, const XferProcessor&, const Texture*,
                       const Point&) {
  }
};
}  // namespace tgfx
