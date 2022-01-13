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

#include "ShaderBuilder.h"
#include "ProgramBuilder.h"
#include "Swizzle.h"
#include "stdarg.h"

namespace pag {
// number of bytes (on the stack) to receive the printf result
static constexpr size_t kBufferSize = 1024;

static bool NeedsAppendEnter(const std::string& code) {
  if (code.empty()) {
    return false;
  }
  auto& ch = code[code.size() - 1];
  return ch == ';' || ch == '{' || ch == '}';
}

ShaderBuilder::ShaderBuilder(ProgramBuilder* program) : programBuilder(program) {
  for (int i = 0; i <= Type::Code; ++i) {
    shaderStrings.emplace_back("");
  }
  shaderStrings[Type::Main] = "void main() {";
  indentation = 1;
  atLineStart = true;
}

void ShaderBuilder::setPrecisionQualifier(const std::string& precision) {
  shaderStrings[Type::PrecisionQualifier] = precision;
}

void ShaderBuilder::codeAppendf(const char* format, ...) {
  char buffer[kBufferSize];
  va_list args;
  va_start(args, format);
  auto length = vsnprintf(buffer, kBufferSize, format, args);
  va_end(args);
  codeAppend(std::string(buffer, length));
}

void ShaderBuilder::codeAppend(const std::string& str) {
  if (str.empty()) {
    return;
  }
  appendIndentationIfNeeded(str);
  auto& code = shaderStrings[Type::Code];
  code += str;
  if (NeedsAppendEnter(code)) {
    appendEnterIfNotEmpty(Type::Code);
  }
  atLineStart = code[code.size() - 1] == '\n';
}

void ShaderBuilder::addFunction(const std::string& str) {
  appendEnterIfNotEmpty(Type::Functions);
  shaderStrings[Type::Functions] += str;
}

static std::string TextureSwizzleString(const Swizzle& swizzle) {
  if (swizzle == Swizzle::RGBA()) {
    return "";
  }
  std::string ret = ".";
  ret.append(swizzle.c_str());
  return ret;
}

void ShaderBuilder::appendTextureLookup(SamplerHandle samplerHandle, const std::string& coordName) {
  const auto& sampler = programBuilder->samplerVariable(samplerHandle);
  codeAppendf("%s(%s, %s)", programBuilder->textureFuncName().c_str(), sampler.name().c_str(),
              coordName.c_str());
  codeAppend(TextureSwizzleString(programBuilder->samplerSwizzle(samplerHandle)));
}

void ShaderBuilder::addFeature(PrivateFeature featureBit, const std::string& extensionName) {
  if ((featureBit & featuresAddedMask) == featureBit) {
    return;
  }
  char buffer[kBufferSize];
  auto length = snprintf(buffer, kBufferSize, "#extension %s: require\n", extensionName.c_str());
  shaderStrings[Type::Extensions].append(buffer, length);
  featuresAddedMask |= featureBit;
}

std::string ShaderBuilder::getDeclarations(const std::vector<ShaderVar>& vars,
                                           ShaderFlags flag) const {
  std::string ret;
  for (const auto& var : vars) {
    ret += programBuilder->getShaderVarDeclarations(var, flag);
    ret += ";\n";
  }
  return ret;
}

void ShaderBuilder::finalize(ShaderFlags visibility) {
  if (finalized) {
    return;
  }
  shaderStrings[Type::VersionDecl] = programBuilder->versionDeclString();
  shaderStrings[Type::Uniforms] += programBuilder->getUniformDeclarations(visibility);
  shaderStrings[Type::Inputs] += getDeclarations(inputs, visibility);
  shaderStrings[Type::Outputs] += getDeclarations(outputs, visibility);
  onFinalize();
  // append the 'footer' to code
  shaderStrings[Type::Code] += "}\n";

  finalized = true;
}

void ShaderBuilder::appendEnterIfNotEmpty(Enum type) {
  if (shaderStrings[type].empty()) {
    return;
  }
  shaderStrings[type] += "\n";
}

void ShaderBuilder::appendIndentationIfNeeded(const std::string& code) {
  if (indentation <= 0 || !atLineStart) {
    return;
  }
  if (code.find('}') != std::string::npos) {
    --indentation;
  }
  for (int i = 0; i < indentation; ++i) {
    shaderStrings[Type::Code] += "    ";
  }
  if (code.find('{') != std::string::npos) {
    ++indentation;
  }
  atLineStart = false;
}

std::string ShaderBuilder::shaderString() {
  std::string fragment;
  for (auto& str : shaderStrings) {
    if (str.empty()) {
      continue;
    }
    fragment += str;
    fragment += "\n";
  }
  return fragment;
}
}  // namespace pag
