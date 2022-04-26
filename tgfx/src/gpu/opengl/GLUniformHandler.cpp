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

#include "GLUniformHandler.h"
#include "GLProgramBuilder.h"
#include "tgfx/gpu/opengl/GLTexture.h"

namespace tgfx {
UniformHandle GLUniformHandler::internalAddUniform(ShaderFlags visibility, ShaderVar::Type type,
                                                   const std::string& name, bool mangleName,
                                                   std::string* outName) {
  Uniform uni;
  uni.variable.setType(type);
  uni.variable.setTypeModifier(ShaderVar::TypeModifier::Uniform);
  char prefix = 'u';
  if (prefix == name[0] || name.find(NO_MANGLE_PREFIX) == 0) {
    prefix = '\0';
  }
  uni.variable.setName(programBuilder->nameVariable(prefix, name, mangleName));
  uni.visibility = visibility;
  uniforms.push_back(std::move(uni));

  auto index = uniforms.size() - 1;
  if (outName) {
    *outName = uniforms[index].variable.name();
  }
  return UniformHandle(index);
}

SamplerHandle GLUniformHandler::addSampler(const TextureSampler* sampler, const std::string& name) {
  auto mangleName = programBuilder->nameVariable('u', name);

  auto caps = GLCaps::Get(programBuilder->getContext());
  const auto& swizzle = caps->getTextureSwizzle(sampler->format);

  ShaderVar::Type type;
  switch (static_cast<const GLSampler*>(sampler)->target) {
    case GL_TEXTURE_EXTERNAL_OES:
      programBuilder->fragmentShaderBuilder()->addFeature(PrivateFeature::OESTexture,
                                                          "GL_OES_EGL_image_external");
      type = ShaderVar::Type::TextureExternalSampler;
      break;
    case GL_TEXTURE_RECTANGLE:
      type = ShaderVar::Type::Texture2DRectSampler;
      break;
    default:
      type = ShaderVar::Type::Texture2DSampler;
      break;
  }
  Uniform samplerUniform;
  samplerUniform.variable.setType(type);
  samplerUniform.variable.setTypeModifier(ShaderVar::TypeModifier::Uniform);
  samplerUniform.variable.setName(mangleName);
  samplerUniform.visibility = ShaderFlags::Fragment;
  samplerSwizzles.push_back(swizzle);
  samplers.push_back(samplerUniform);
  return SamplerHandle(samplers.size() - 1);
}

std::string GLUniformHandler::getUniformDeclarations(ShaderFlags visibility) const {
  std::string ret;
  for (const auto& uniform : uniforms) {
    if ((uniform.visibility & visibility) == visibility) {
      ret += programBuilder->getShaderVarDeclarations(uniform.variable, visibility);
      ret += ";\n";
    }
  }
  for (const auto& sampler : samplers) {
    if ((sampler.visibility & visibility) == visibility) {
      ret += programBuilder->getShaderVarDeclarations(sampler.variable, visibility);
      ret += ";\n";
    }
  }
  return ret;
}

void GLUniformHandler::resolveUniformLocations(unsigned programID) {
  auto gl = GLFunctions::Get(programBuilder->getContext());
  for (auto& uniform : uniforms) {
    uniform.location = gl->getUniformLocation(programID, uniform.variable.name().c_str());
  }
  for (auto& sampler : samplers) {
    sampler.location = gl->getUniformLocation(programID, sampler.variable.name().c_str());
  }
}
}  // namespace tgfx
