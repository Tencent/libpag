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

#include "pagx/nodes/ColorSource.h"
#include "pagx/types/FilterMode.h"
#include "pagx/types/Matrix.h"
#include "pagx/types/MipmapMode.h"
#include "pagx/types/ScaleMode.h"
#include "pagx/types/TileMode.h"

namespace pagx {

class Image;

/**
 * An image pattern color source that uses an image as a fill. The image is first transformed by
 * the matrix attribute in its local coordinate space (the original image rect with the origin at
 * its top-left). The transformed image is then placed into the geometry according to scaleMode.
 */
class ImagePattern : public ColorSource {
 public:
  /**
   * A reference to an image resource.
   */
  Image* image = nullptr;

  /**
   * The tile mode for the horizontal direction. The default value is Decal.
   */
  TileMode tileModeX = TileMode::Decal;

  /**
   * The tile mode for the vertical direction. The default value is Decal.
   */
  TileMode tileModeY = TileMode::Decal;

  /**
   * The filter mode for texture sampling. The default value is Linear.
   */
  FilterMode filterMode = FilterMode::Linear;

  /**
   * The mipmap mode for texture sampling. The default value is Linear.
   */
  MipmapMode mipmapMode = MipmapMode::Linear;

  /**
   * The transformation matrix applied to the pattern. The matrix operates on the image's local
   * coordinate space (the original image rect with the origin at its top-left).
   */
  Matrix matrix = {};

  /**
   * The rule used to place the transformed image into the geometry. The default is LetterBox,
   * which fits the image into each geometry's bounding box (so the fill auto-fits per geometry).
   * When set to ScaleMode::None, the image is placed in the parent container's coordinate space
   * (the owning Group or Layer, with its origin at (0, 0)) without per-geometry fitting, and
   * multiple geometries in that container share one continuous fill.
   */
  ScaleMode scaleMode = ScaleMode::LetterBox;

  NodeType nodeType() const override {
    return NodeType::ImagePattern;
  }

 private:
  ImagePattern() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
