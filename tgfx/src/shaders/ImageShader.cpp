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
#include "images/ImageSource.h"

namespace tgfx {
std::shared_ptr<Shader> Shader::MakeImageShader(std::shared_ptr<Image> image, TileMode tileModeX,
                                                TileMode tileModeY, SamplingOptions sampling) {
  if (image == nullptr) {
    return nullptr;
  }
  auto shader = std::shared_ptr<ImageShader>(
      new ImageShader(std::move(image), tileModeX, tileModeY, sampling));
  shader->weakThis = shader;
  return shader;
}

std::unique_ptr<FragmentProcessor> ImageShader::asFragmentProcessor(const FPArgs& args) const {
  auto matrix = Matrix::I();
  if (!ComputeTotalInverse(args, &matrix)) {
    return nullptr;
  }
  auto processor = image->asFragmentProcessor(args.context, args.surfaceFlags, tileModeX, tileModeY,
                                              sampling, &matrix);
  if (processor == nullptr) {
    return nullptr;
  }
  if (image->isAlphaOnly()) {
    return processor;
  }
  return FragmentProcessor::MulChildByInputAlpha(std::move(processor));
}
}  // namespace tgfx
