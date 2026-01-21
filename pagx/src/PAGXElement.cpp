/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "pagx/nodes/VectorElement.h"

namespace pagx {

const char* VectorElementTypeName(VectorElementType type) {
  switch (type) {
    case VectorElementType::Rectangle:
      return "Rectangle";
    case VectorElementType::Ellipse:
      return "Ellipse";
    case VectorElementType::Polystar:
      return "Polystar";
    case VectorElementType::Path:
      return "Path";
    case VectorElementType::TextSpan:
      return "TextSpan";
    case VectorElementType::Fill:
      return "Fill";
    case VectorElementType::Stroke:
      return "Stroke";
    case VectorElementType::TrimPath:
      return "TrimPath";
    case VectorElementType::RoundCorner:
      return "RoundCorner";
    case VectorElementType::MergePath:
      return "MergePath";
    case VectorElementType::TextModifier:
      return "TextModifier";
    case VectorElementType::TextPath:
      return "TextPath";
    case VectorElementType::TextLayout:
      return "TextLayout";
    case VectorElementType::Group:
      return "Group";
    case VectorElementType::Repeater:
      return "Repeater";
    default:
      return "Unknown";
  }
}

}  // namespace pagx
