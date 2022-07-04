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

#include "gpu/FragmentProcessor.h"
#include "tgfx/core/RGBAAALayout.h"

namespace tgfx {
class TextureEffect : public FragmentProcessor {
 public:
  static std::unique_ptr<FragmentProcessor> Make(const Texture* texture,
                                                 const Matrix& localMatrix = Matrix::I(),
                                                 const RGBAAALayout* layout = nullptr);

  std::string name() const override {
    return "TextureEffect";
  }

 private:
  explicit TextureEffect(const Texture* texture, const RGBAAALayout* layout,
                         const Matrix& localMatrix);

  void onComputeProcessorKey(BytesKey* bytesKey) const override;

  std::unique_ptr<GLFragmentProcessor> onCreateGLInstance() const override;

  const TextureSampler* onTextureSampler(size_t) const override {
    return texture->getSampler();
  }

  const Texture* texture;
  const RGBAAALayout* layout;
  CoordTransform coordTransform;

  friend class GLTextureEffect;
};
}  // namespace tgfx
