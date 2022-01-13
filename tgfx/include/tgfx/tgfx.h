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

#include <memory>

namespace tgfx {
/**
 * Rasterizer is a utility that can take an image described in a vector graphics format (paths,
 * glyphs) and convert it into a raster image.
 */
class Rasterizer {
 public:
  static std::shared_ptr<const Rasterizer> MakeFreeType();

  static std::shared_ptr<const Rasterizer> MakeNative();

  virtual ~Rasterizer() = default;
};

class GLInterface {
 public:
  static std::shared_ptr<const GLInterface> MakeEGL();

  static std::shared_ptr<const GLInterface> MakeQT();

  static std::shared_ptr<const GLInterface> MakeNative();

  virtual ~GLInterface() = default;
};

class Context {
 public:
  static std::shared_ptr<Context> MakeGL();

  static std::shared_ptr<Context> MakeGL(static std::shared_ptr<const Rasterizer>,
                                         std::shared_ptr<const GrGLInterface> glInterface);

  virtual ~Context() = default;
};
}  // namespace tgfx
