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

#include "TextureGradientColorizer.h"
#include "core/utils/UniqueID.h"
#include "gpu/opengl/GLTextureGradientColorizer.h"

namespace tgfx {
std::unique_ptr<TextureGradientColorizer> TextureGradientColorizer::Make(
    std::shared_ptr<Texture> gradient) {
  return std::unique_ptr<TextureGradientColorizer>(
      new TextureGradientColorizer(std::move(gradient)));
}

void TextureGradientColorizer::onComputeProcessorKey(BytesKey* bytesKey) const {
  static auto Type = UniqueID::Next();
  bytesKey->write(Type);
}

std::unique_ptr<GLFragmentProcessor> TextureGradientColorizer::onCreateGLInstance() const {
  return std::make_unique<GLTextureGradientColorizer>();
}
}  // namespace tgfx
