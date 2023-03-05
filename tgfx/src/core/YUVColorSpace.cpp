/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "tgfx/core/YUVColorSpace.h"

namespace tgfx {
bool IsLimitedYUVColorRange(YUVColorSpace colorSpace) {
  switch (colorSpace) {
    case YUVColorSpace::BT601_LIMITED:
    case YUVColorSpace::BT709_LIMITED:
    case YUVColorSpace::BT2020_LIMITED:
      return true;
    default:
      break;
  }
  return false;
}
}  // namespace tgfx
