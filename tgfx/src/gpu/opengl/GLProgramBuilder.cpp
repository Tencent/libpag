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

#include "GLProgramBuilder.h"
#include "GLContext.h"
#include "GLUtil.h"

namespace pag {
static std::string TypeModifierString(bool isDesktopGL, ShaderVar::TypeModifier t,
                                      ShaderFlags flag) {
  switch (t) {
    case ShaderVar::TypeModifier::None:
      return "";
    case ShaderVar::TypeModifier::Attribute:
      return isDesktopGL ? "in" : "attribute";
    case ShaderVar::TypeModifier::Varying:
      return isDesktopGL ? (flag == ShaderFlags::Vertex ? "out" : "in") : "varying";
    case ShaderVar::TypeModifier::Uniform:
      return "uniform";
    case ShaderVar::TypeModifier::Out:
      return "out";
  }
}

static constexpr std::pair<ShaderVar::Type, const char*> kSLTypes[] = {
    {ShaderVar::Type::Void, "void"},
    {ShaderVar::Type::Float, "float"},
    {ShaderVar::Type::Float2, "vec2"},
    {ShaderVar::Type::Float3, "vec3"},
    {ShaderVar::Type::Float4, "vec4"},
    {ShaderVar::Type::Float3x3, "mat3"},
    {ShaderVar::Type::Texture2DRectSampler, "sampler2DRect"},
    {ShaderVar::Type::TextureExternalSampler, "samplerExternalOES"},
    {ShaderVar::Type::Texture2DSampler, "sampler2D"},
};

static std::string SLTypeString(ShaderVar::Type t) {
  for (const auto& pair : kSLTypes) {
    if (pair.first == t) {
      return pair.second;
    }
  }
  return "";
}

std::unique_ptr<GLProgram> GLProgramBuilder::CreateProgram(
    const GLInterface* gl, const GeometryProcessor* geometryProcessor, const Pipeline* pipeline) {
  GLProgramBuilder builder(gl, geometryProcessor, pipeline);
  if (!builder.emitAndInstallProcessors()) {
    return nullptr;
  }
  return builder.finalize();
}

GLProgramBuilder::GLProgramBuilder(const GLInterface* gl,
                                   const GeometryProcessor* geometryProcessor,
                                   const Pipeline* pipeline)
    : ProgramBuilder(geometryProcessor, pipeline),
      _gl(gl),
      _varyingHandler(this),
      _uniformHandler(this),
      _vertexBuilder(this),
      _fragBuilder(this) {
}

std::string GLProgramBuilder::versionDeclString() {
  return isDesktopGL() ? "#version 150\n" : "#version 100\n";
}

std::string GLProgramBuilder::textureFuncName() const {
  return isDesktopGL() ? "texture" : "texture2D";
}

std::string GLProgramBuilder::getShaderVarDeclarations(const ShaderVar& var,
                                                       ShaderFlags flag) const {
  std::string ret;
  if (var.typeModifier() != ShaderVar::TypeModifier::None) {
    ret += TypeModifierString(isDesktopGL(), var.typeModifier(), flag);
    ret += " ";
    // On Android，fragment shader's varying needs high precision.
    // test file: assets/TZ2.pag
    if (var.typeModifier() == ShaderVar::TypeModifier::Varying && flag == ShaderFlags::Fragment) {
      ret += "highp ";
    }
  }
  ret += SLTypeString(var.type());
  ret += " ";
  ret += var.name();
  return ret;
}

std::unique_ptr<GLProgram> GLProgramBuilder::finalize() {
  if (isDesktopGL()) {
    fragmentShaderBuilder()->declareCustomOutputColor();
  }
  finalizeShaders();

  auto vertex = vertexShaderBuilder()->shaderString();
  auto fragment = fragmentShaderBuilder()->shaderString();
  auto programID = pag::CreateProgram(_gl, vertex, fragment);
  if (programID == 0) {
    return nullptr;
  }
  computeCountsAndStrides(programID);
  resolveProgramResourceLocations(programID);

  return createProgram(programID);
}

void GLProgramBuilder::computeCountsAndStrides(unsigned int programID) {
  vertexStride = 0;
  for (const auto* attr : geometryProcessor->vertexAttributes()) {
    GLProgram::Attribute attribute;
    attribute.gpuType = attr->gpuType();
    attribute.offset = vertexStride;
    vertexStride += static_cast<int>(attr->sizeAlign4());
    attribute.location = _gl->getAttribLocation(programID, attr->name().c_str());
    if (attribute.location >= 0) {
      attributes.push_back(attribute);
    }
  }
}

void GLProgramBuilder::resolveProgramResourceLocations(unsigned programID) {
  _uniformHandler.resolveUniformLocations(programID);
}

bool GLProgramBuilder::checkSamplerCounts() {
  if (numFragmentSamplers > _gl->caps->maxFragmentSamplers) {
    LOGE("Program would use too many fragment samplers.");
    return false;
  }
  return true;
}

std::unique_ptr<GLProgram> GLProgramBuilder::createProgram(unsigned programID) {
  auto program = new GLProgram(programID, _uniformHandler.uniforms, std::move(glGeometryProcessor),
                               std::move(xferProcessor), std::move(fragmentProcessors), attributes,
                               vertexStride);
  program->setupSamplerUniforms(_gl, _uniformHandler.samplers);
  return std::unique_ptr<GLProgram>(program);
}

bool GLProgramBuilder::isDesktopGL() const {
  return gl()->caps->standard == GLStandard::GL;
}
}  // namespace pag
