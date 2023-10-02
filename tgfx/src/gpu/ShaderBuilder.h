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

#include "SamplerHandle.h"
#include "ShaderVar.h"

namespace tgfx {
class ProgramBuilder;

/**
 * Features that should only be enabled internally by the builders.
 */
enum class PrivateFeature : unsigned {
  None = 0,
  OESTexture = 1 << 0,
  FramebufferFetch = 1 << 1,
  TGFX_MARK_AS_BITMASK_ENUM(FramebufferFetch)
};

class ShaderBuilder {
 public:
  explicit ShaderBuilder(ProgramBuilder* program);

  virtual ~ShaderBuilder() = default;

  void setPrecisionQualifier(const std::string& precision);

  /**
   * Appends a 2D texture sample. The vec length and swizzle order of the result depends on the
   * TextureSampler associated with the SamplerHandle.
   */
  void appendTextureLookup(SamplerHandle samplerHandle, const std::string& coordName);

  /**
   * Called by Processors to add code to one of the shaders.
   */
  void codeAppendf(const char format[], ...);

  void codeAppend(const std::string& str);

  void addFunction(const std::string& str);

  /**
   * Combines the various parts of the shader to create a single finalized shader string.
   */
  void finalize(ShaderFlags visibility);

  std::string shaderString();

 protected:
  class Type {
   public:
    static const uint8_t VersionDecl = 0;
    static const uint8_t Extensions = 1;
    static const uint8_t Definitions = 2;
    static const uint8_t PrecisionQualifier = 3;
    static const uint8_t Uniforms = 4;
    static const uint8_t Inputs = 5;
    static const uint8_t Outputs = 6;
    static const uint8_t Functions = 7;
    static const uint8_t Main = 8;
    static const uint8_t Code = 9;
  };

  /**
   * A general function which enables an extension in a shader if the feature bit is not present
   */
  void addFeature(PrivateFeature featureBit, const std::string& extensionName);

  void nextStage() {
    codeIndex++;
  }

  virtual void onFinalize() = 0;

  void appendEnterIfNotEmpty(uint8_t type);

  void appendIndentationIfNeeded(const std::string& code);

  std::string getDeclarations(const std::vector<ShaderVar>& vars, ShaderFlags flag) const;

  std::vector<std::string> shaderStrings;
  ProgramBuilder* programBuilder = nullptr;
  std::vector<ShaderVar> inputs;
  std::vector<ShaderVar> outputs;
  PrivateFeature featuresAddedMask = PrivateFeature::None;
  int codeIndex = 0;
  bool finalized = false;
  int indentation = 0;
  bool atLineStart = false;

  friend class ProgramBuilder;

  friend class GLUniformHandler;
};
}  // namespace tgfx
