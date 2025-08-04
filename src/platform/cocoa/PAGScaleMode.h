/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

typedef enum {
  // The content is not scaled.
  PAGScaleModeNone = 0,
  // The content is stretched to fit.
  PAGScaleModeStretch = 1,
  // The content is scaled with respect to the original unscaled image's aspect ratio. This is the
  // default value.
  PAGScaleModeLetterBox = 2,
  // The content is scaled to fit with respect to the original unscaled image's aspect ratio. This
  // results in cropping
  // on one axis.
  PAGScaleModeZoom = 3,
} PAGScaleMode;
