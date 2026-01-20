/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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
#include <string>
#include "tgfx/core/Stream.h"
#include "tgfx/layers/Layer.h"

namespace pagx {

/**
 * PAGXContent represents the result of parsing a PAGX file, containing the root layer
 * and canvas dimensions.
 */
struct PAGXContent {
  /**
   * The root layer of the PAGX content. nullptr if parsing failed.
   */
  std::shared_ptr<tgfx::Layer> root = nullptr;
  /**
   * The canvas width specified in the PAGX file.
   */
  float width = 0.0f;
  /**
   * The canvas height specified in the PAGX file.
   */
  float height = 0.0f;
};

/**
 * LayerBuilder provides functionality to build tgfx vector layer trees from PAGX files.
 * PAGX (Portable Animated Graphics XML) is an XML-based markup language for describing
 * animated vector graphics.
 */
class LayerBuilder {
 public:
  /**
   * Builds a Layer tree from a PAGX file.
   * @param filePath The path to the PAGX file. The file's directory is used as the base path
   *                 for resolving relative resource paths (e.g., images).
   * @return PAGXContent containing the root layer and canvas dimensions.
   */
  static PAGXContent FromFile(const std::string& filePath);

  /**
   * Builds a Layer tree from a stream containing PAGX XML data.
   * @param stream The stream containing PAGX XML data.
   * @param basePath The base directory path for resolving relative resource paths.
   *                 If empty, relative paths cannot be resolved.
   * @return PAGXContent containing the root layer and canvas dimensions.
   */
  static PAGXContent FromStream(tgfx::Stream& stream, const std::string& basePath = "");
};

}  // namespace pagx
