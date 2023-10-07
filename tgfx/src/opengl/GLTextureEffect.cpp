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

#include "GLTextureEffect.h"
#include "gpu/TextureEffect.h"
#include "opengl/GLYUVTextureEffect.h"

namespace tgfx {
std::unique_ptr<FragmentProcessor> TextureEffect::Make(std::shared_ptr<Texture> texture,
                                                       const SamplingOptions& sampling,
                                                       const Matrix* localMatrix) {
  return MakeRGBAAA(std::move(texture), sampling, Point::Zero(), localMatrix);
}

std::unique_ptr<FragmentProcessor> TextureEffect::MakeRGBAAA(std::shared_ptr<Texture> texture,
                                                             const SamplingOptions& sampling,
                                                             const Point& alphaStart,
                                                             const Matrix* localMatrix) {
  if (texture == nullptr) {
    return nullptr;
  }
  auto matrix = localMatrix ? *localMatrix : Matrix::I();
  if (texture->isYUV()) {
    return std::unique_ptr<YUVTextureEffect>(new GLYUVTextureEffect(
        std::static_pointer_cast<YUVTexture>(texture), sampling, alphaStart, matrix));
  }
  return std::unique_ptr<TextureEffect>(
      new GLTextureEffect(std::move(texture), sampling, alphaStart, matrix));
}

GLTextureEffect::GLTextureEffect(std::shared_ptr<Texture> texture, SamplingOptions sampling,
                                 const Point& alphaStart, const Matrix& localMatrix)
    : TextureEffect(std::move(texture), sampling, alphaStart, localMatrix) {
}

void GLTextureEffect::emitCode(EmitArgs& args) const {
  auto* fragBuilder = args.fragBuilder;
  auto* uniformHandler = args.uniformHandler;

  auto vertexColor = (*args.transformedCoords)[0].name();
  if (args.coordFunc) {
    vertexColor = args.coordFunc(vertexColor);
  }
  fragBuilder->codeAppend("vec4 color = ");
  fragBuilder->appendTextureLookup((*args.textureSamplers)[0], vertexColor);
  fragBuilder->codeAppend(";");
  if (alphaStart != Point::Zero()) {
    fragBuilder->codeAppend("color = clamp(color, 0.0, 1.0);");
    auto alphaStartName =
        uniformHandler->addUniform(ShaderFlags::Fragment, ShaderVar::Type::Float2, "alphaStart");
    std::string alphaVertexColor = "alphaVertexColor";
    fragBuilder->codeAppendf("vec2 %s = %s + %s;", alphaVertexColor.c_str(), vertexColor.c_str(),
                             alphaStartName.c_str());
    fragBuilder->codeAppend("vec4 alpha = ");
    fragBuilder->appendTextureLookup((*args.textureSamplers)[0], alphaVertexColor);
    fragBuilder->codeAppend(";");
    fragBuilder->codeAppend("alpha = clamp(alpha, 0.0, 1.0);");
    fragBuilder->codeAppend("color = vec4(color.rgb * alpha.r, alpha.r);");
  }
  fragBuilder->codeAppendf("%s = color * %s;", args.outputColor.c_str(), args.inputColor.c_str());
}

void GLTextureEffect::onSetData(UniformBuffer* uniformBuffer) const {
  if (alphaStart != Point::Zero()) {
    auto alphaStartValue = texture->getTextureCoord(alphaStart.x, alphaStart.y);
    uniformBuffer->setData("alphaStart", &alphaStartValue);
  }
}
}  // namespace tgfx
