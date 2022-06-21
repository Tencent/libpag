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

#include "TextureShader.h"
#include "TextureEffect.h"
#include "tgfx/gpu/TextureSampler.h"

namespace tgfx {
std::shared_ptr<Shader> Shader::MakeTextureShader(std::shared_ptr<Texture> texture) {
  return TextureShader::Make(std::move(texture));
}

std::shared_ptr<Shader> TextureShader::Make(std::shared_ptr<Texture> texture) {
  if (texture == nullptr) {
    return nullptr;
  }
  auto shader = std::shared_ptr<TextureShader>(new TextureShader(std::move(texture)));
  shader->weakThis = shader;
  return shader;
}

std::unique_ptr<FragmentProcessor> TextureShader::asFragmentProcessor(const FPArgs& args) const {
  auto matrix = Matrix::I();
  if (!ComputeTotalInverse(args, &matrix)) {
    return nullptr;
  }
  auto effect = TextureEffect::Make(texture.get(), matrix);
  if (texture->getSampler()->format == PixelFormat::ALPHA_8) {
    return effect;
  }
  return FragmentProcessor::MulChildByInputAlpha(std::move(effect));
}
}  // namespace tgfx
