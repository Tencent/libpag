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

#include "FragmentProcessor.h"

namespace tgfx {
class TextureMaskFragmentProcessor : public FragmentProcessor {
 public:
  static std::unique_ptr<TextureMaskFragmentProcessor> MakeUseLocalCoord(
      const Texture* texture, const Matrix& localMatrix = Matrix::I(), bool inverted = false,
      bool useLumaMatte = false);

  static std::unique_ptr<TextureMaskFragmentProcessor> MakeUseDeviceCoord(const Texture* texture,
                                                                          ImageOrigin deviceOrigin);

  std::string name() const override {
    return "TextureMaskFragmentProcessor";
  }

 private:
  TextureMaskFragmentProcessor(const Texture* texture, ImageOrigin deviceOrigin);

  TextureMaskFragmentProcessor(const Texture* texture, const Matrix& localMatrix, bool inverted,
                               bool useLumaMatte);

  void onComputeProcessorKey(BytesKey* bytesKey) const override;

  std::unique_ptr<GLFragmentProcessor> onCreateGLInstance() const override;

  const TextureSampler* onTextureSampler(size_t) const override {
    return texture->getSampler();
  }

  bool useLocalCoord;
  const Texture* texture;
  CoordTransform coordTransform;
  bool inverted = false;
  bool useLumaMatte = false;
  Matrix deviceCoordMatrix = Matrix::I();

  friend class GLTextureMaskFragmentProcessor;
};
}  // namespace tgfx
