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

#include "GLTextureGradientColorizer.h"

namespace tgfx {
std::unique_ptr<TextureGradientColorizer> TextureGradientColorizer::Make(
    std::shared_ptr<Texture> gradient) {
  return std::unique_ptr<TextureGradientColorizer>(
      new GLTextureGradientColorizer(std::move(gradient)));
}

GLTextureGradientColorizer::GLTextureGradientColorizer(std::shared_ptr<Texture> gradient)
    : TextureGradientColorizer(std::move(gradient)) {
}

void GLTextureGradientColorizer::emitCode(EmitArgs& args) const {
  auto* fragBuilder = args.fragBuilder;
  fragBuilder->codeAppendf("vec2 coord = vec2(%s.x, 0.5);", args.inputColor.c_str());
  fragBuilder->codeAppendf("%s = ", args.outputColor.c_str());
  fragBuilder->appendTextureLookup((*args.textureSamplers)[0], "coord");
  fragBuilder->codeAppend(";");
}
}  // namespace tgfx
