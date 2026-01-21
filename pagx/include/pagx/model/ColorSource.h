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

namespace pagx {

/**
 * Color source types.
 */
enum class ColorSourceType {
  SolidColor,
  LinearGradient,
  RadialGradient,
  ConicGradient,
  DiamondGradient,
  ImagePattern
};

/**
 * Returns the string name of a color source type.
 */
const char* ColorSourceTypeName(ColorSourceType type);

/**
 * Base class for color sources (SolidColor, gradients, ImagePattern).
 * ColorSource can be used both inline in painters and as standalone resources in defs.
 */
class ColorSource {
 public:
  virtual ~ColorSource() = default;

  /**
   * Returns the color source type of this color source.
   */
  virtual ColorSourceType type() const = 0;

 protected:
  ColorSource() = default;
};

}  // namespace pagx
