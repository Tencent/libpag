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

#include "pag/types.h"

namespace pag {
enum class Blend {
  Clear,                   //!< replaces destination with zero: fully transparent
  Src,                     //!< replaces destination
  Dst,                     //!< preserves destination
  SrcOver,                 //!< source over destination
  DstOver,                 //!< destination over source
  SrcIn,                   //!< source trimmed inside destination
  DstIn,                   //!< destination trimmed by source
  SrcOut,                  //!< source trimmed outside destination
  DstOut,                  //!< destination trimmed outside source
  SrcATop,                 //!< source inside destination blended with destination
  DstATop,                 //!< destination inside source blended with source
  Xor,                     //!< each of source and destination trimmed outside the other
  Plus,                    //!< sum of colors
  Modulate,                //!< product of premultiplied colors; darkens destination
  Screen,                  //!< multiply inverse of pixels, inverting result; brightens destination
  LastCoeffMode = Screen,  //!< last porter duff blend mode
  Overlay,                 //!< multiply or screen, depending on destination
  Darken,                  //!< darker of source and destination
  Lighten,                 //!< lighter of source and destination
  ColorDodge,              //!< brighten destination to reflect source
  ColorBurn,               //!< darken destination to reflect source
  HardLight,               //!< multiply or screen, depending on source
  SoftLight,               //!< lighten or darken, depending on source
  Difference,              //!< subtract darker from lighter with higher contrast
  Exclusion,               //!< subtract darker from lighter with lower contrast
  Multiply,                //!< multiply source with destination, darkening image
  LastSeparableMode = Multiply,  //!< last blend mode operating separately on components
  Hue,                           //!< hue of source with saturation and luminosity of destination
  Saturation,                    //!< saturation of source with hue and luminosity of destination
  Color,                         //!< hue and saturation of source with luminosity of destination
  Luminosity,                    //!< luminosity of source with hue and saturation of destination
  LastMode = Luminosity,         //!< last valid value
};

Blend ToBlend(Enum blendMode);
}  // namespace pag
