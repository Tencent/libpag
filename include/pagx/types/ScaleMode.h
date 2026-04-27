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

namespace pagx {

/**
 * ScaleMode controls how an ImagePattern image is fitted into the geometry's bounding box.
 */
enum class ScaleMode {
  /**
   * The image is not fitted into the geometry's bounding box. It is placed in the parent
   * container's (Layer or Group) coordinate space (origin at (0, 0)) and extended outside the
   * image bounds according to the pattern's tile modes. Use this mode to share one continuous
   * image layout across multiple geometries instead of giving each its own fitted copy.
   */
  None,

  /**
   * The image is stretched to fill the bounding box, ignoring its original aspect ratio.
   */
  Stretch,

  /**
   * The image is scaled uniformly to fit inside the bounding box while preserving its aspect
   * ratio. The image is centered and empty areas may appear along one axis. This is the default
   * value.
   */
  LetterBox,

  /**
   * The image is scaled uniformly to cover the bounding box while preserving its aspect ratio.
   * The image is centered and may be cropped along one axis.
   */
  Zoom
};

}  // namespace pagx
