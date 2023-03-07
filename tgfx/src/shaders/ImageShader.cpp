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

#include "ImageShader.h"
#include "gpu/TextureSampler.h"
#include "gpu/TiledTextureEffect.h"

namespace tgfx {
std::shared_ptr<Shader> Shader::MakeImageShader(std::shared_ptr<Texture> texture,
                                                TileMode tileModeX, TileMode tileModeY,
                                                SamplingOptions sampling) {
  return ImageShader::Make(std::move(texture), tileModeX, tileModeY, sampling);
}

std::shared_ptr<Shader> ImageShader::Make(std::shared_ptr<Texture> texture, TileMode tileModeX,
                                          TileMode tileModeY, SamplingOptions sampling) {
  if (texture == nullptr) {
    return nullptr;
  }
  auto shader = std::shared_ptr<ImageShader>(
      new ImageShader(std::move(texture), tileModeX, tileModeY, sampling));
  shader->weakThis = shader;
  return shader;
}

std::unique_ptr<FragmentProcessor> ImageShader::asFragmentProcessor(const FPArgs& args) const {
  auto matrix = Matrix::I();
  if (!ComputeTotalInverse(args, &matrix)) {
    return nullptr;
  }
  auto effect =
      TiledTextureEffect::Make(texture, SamplerState(tileModeX, tileModeY, sampling), &matrix);
  if (texture->getSampler()->format == PixelFormat::ALPHA_8) {
    return effect;
  }
  return FragmentProcessor::MulChildByInputAlpha(std::move(effect));
}
}  // namespace tgfx
