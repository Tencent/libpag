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

#include "pagx/model/ColorSource.h"
#include "pagx/model/Element.h"
#include "pagx/model/LayerFilter.h"
#include "pagx/model/LayerStyle.h"
#include "pagx/model/Node.h"

namespace pagx {

const char* NodeTypeName(NodeType type) {
  switch (type) {
    case NodeType::Image:
      return "Image";
    case NodeType::PathData:
      return "PathData";
    case NodeType::Composition:
      return "Composition";
    default:
      return "Unknown";
  }
}

const char* ElementTypeName(ElementType type) {
  switch (type) {
    case ElementType::Rectangle:
      return "Rectangle";
    case ElementType::Ellipse:
      return "Ellipse";
    case ElementType::Polystar:
      return "Polystar";
    case ElementType::Path:
      return "Path";
    case ElementType::TextSpan:
      return "TextSpan";
    case ElementType::Fill:
      return "Fill";
    case ElementType::Stroke:
      return "Stroke";
    case ElementType::TrimPath:
      return "TrimPath";
    case ElementType::RoundCorner:
      return "RoundCorner";
    case ElementType::MergePath:
      return "MergePath";
    case ElementType::TextModifier:
      return "TextModifier";
    case ElementType::TextPath:
      return "TextPath";
    case ElementType::TextLayout:
      return "TextLayout";
    case ElementType::Group:
      return "Group";
    case ElementType::Repeater:
      return "Repeater";
    default:
      return "Unknown";
  }
}

const char* ColorSourceTypeName(ColorSourceType type) {
  switch (type) {
    case ColorSourceType::SolidColor:
      return "SolidColor";
    case ColorSourceType::LinearGradient:
      return "LinearGradient";
    case ColorSourceType::RadialGradient:
      return "RadialGradient";
    case ColorSourceType::ConicGradient:
      return "ConicGradient";
    case ColorSourceType::DiamondGradient:
      return "DiamondGradient";
    case ColorSourceType::ImagePattern:
      return "ImagePattern";
    default:
      return "Unknown";
  }
}

const char* LayerStyleTypeName(LayerStyleType type) {
  switch (type) {
    case LayerStyleType::DropShadowStyle:
      return "DropShadowStyle";
    case LayerStyleType::InnerShadowStyle:
      return "InnerShadowStyle";
    case LayerStyleType::BackgroundBlurStyle:
      return "BackgroundBlurStyle";
    default:
      return "Unknown";
  }
}

const char* LayerFilterTypeName(LayerFilterType type) {
  switch (type) {
    case LayerFilterType::BlurFilter:
      return "BlurFilter";
    case LayerFilterType::DropShadowFilter:
      return "DropShadowFilter";
    case LayerFilterType::InnerShadowFilter:
      return "InnerShadowFilter";
    case LayerFilterType::BlendFilter:
      return "BlendFilter";
    case LayerFilterType::ColorMatrixFilter:
      return "ColorMatrixFilter";
    default:
      return "Unknown";
  }
}

}  // namespace pagx
