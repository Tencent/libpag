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

#import "platform/cocoa/private/ColorUtility.h"

pag::Color ColorUtility::ToColor(CocoaColor* color) {
  if (color == nil) {
    return pag::Black;
  }
  CGFloat red, green, blue, alpha;
  if (![color getRed:&red green:&green blue:&blue alpha:&alpha]) {
    [color getWhite:&red alpha:&alpha];
    green = red;
    blue = red;
  };
  return {static_cast<uint8_t>(red * 255), static_cast<uint8_t>(green * 255),
          static_cast<uint8_t>(blue * 255)};
}
