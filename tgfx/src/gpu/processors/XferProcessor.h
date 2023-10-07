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

#include <memory>
#include "gpu/FragmentShaderBuilder.h"
#include "gpu/Texture.h"
#include "gpu/UniformBuffer.h"
#include "gpu/UniformHandler.h"
#include "gpu/processors/Processor.h"
#include "tgfx/utils/BytesKey.h"

namespace tgfx {
class XferProcessor : public Processor {
 public:
  struct EmitArgs {
    EmitArgs(FragmentShaderBuilder* fragBuilder, UniformHandler* uniformHandler,
             std::string inputColor, std::string inputCoverage, std::string outputColor,
             const SamplerHandle dstTextureSamplerHandle)
        : fragBuilder(fragBuilder),
          uniformHandler(uniformHandler),
          inputColor(std::move(inputColor)),
          inputCoverage(std::move(inputCoverage)),
          outputColor(std::move(outputColor)),
          dstTextureSamplerHandle(dstTextureSamplerHandle) {
    }
    FragmentShaderBuilder* fragBuilder;
    UniformHandler* uniformHandler;
    const std::string inputColor;
    const std::string inputCoverage;
    const std::string outputColor;
    const SamplerHandle dstTextureSamplerHandle;
  };

  virtual void emitCode(const EmitArgs& args) const = 0;

  virtual void setData(UniformBuffer*, const Texture*, const Point&) const {
  }

 protected:
  explicit XferProcessor(uint32_t classID) : Processor(classID) {
  }
};
}  // namespace tgfx
