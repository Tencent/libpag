/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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

export enum PAGScaleMode {
  /**
   * The content is not scaled.
   */
  None = 0,
  /**
   * The content is stretched to fit.
   */
  Stretch = 1,
  /**
   * The content is scaled with respect to the original unscaled image's aspect ratio.
   * This is the default value.
   */
  LetterBox = 2,
  /**
   * The content is scaled to fit with respect to the original unscaled image's aspect ratio.
   * This results in cropping on one axis.
   */
  Zoom = 3
}
