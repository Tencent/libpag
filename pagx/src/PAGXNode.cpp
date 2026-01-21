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

#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/Element.h"
#include "pagx/nodes/LayerFilter.h"
#include "pagx/nodes/LayerStyle.h"
#include "pagx/nodes/Node.h"

namespace pagx {

const char* NodeTypeName(NodeType type) {
  switch (type) {
    case NodeType::SolidColor:
      return "SolidColor";
    case NodeType::LinearGradient:
      return "LinearGradient";
    case NodeType::RadialGradient:
      return "RadialGradient";
    case NodeType::ConicGradient:
      return "ConicGradient";
    case NodeType::DiamondGradient:
      return "DiamondGradient";
    case NodeType::ImagePattern:
      return "ImagePattern";
    case NodeType::ColorStop:
      return "ColorStop";
    case NodeType::Rectangle:
      return "Rectangle";
    case NodeType::Ellipse:
      return "Ellipse";
    case NodeType::Polystar:
      return "Polystar";
    case NodeType::Path:
      return "Path";
    case NodeType::TextSpan:
      return "TextSpan";
    case NodeType::Fill:
      return "Fill";
    case NodeType::Stroke:
      return "Stroke";
    case NodeType::TrimPath:
      return "TrimPath";
    case NodeType::RoundCorner:
      return "RoundCorner";
    case NodeType::MergePath:
      return "MergePath";
    case NodeType::TextModifier:
      return "TextModifier";
    case NodeType::TextPath:
      return "TextPath";
    case NodeType::TextLayout:
      return "TextLayout";
    case NodeType::RangeSelector:
      return "RangeSelector";
    case NodeType::Repeater:
      return "Repeater";
    case NodeType::Group:
      return "Group";
    case NodeType::DropShadowStyle:
      return "DropShadowStyle";
    case NodeType::InnerShadowStyle:
      return "InnerShadowStyle";
    case NodeType::BackgroundBlurStyle:
      return "BackgroundBlurStyle";
    case NodeType::BlurFilter:
      return "BlurFilter";
    case NodeType::DropShadowFilter:
      return "DropShadowFilter";
    case NodeType::InnerShadowFilter:
      return "InnerShadowFilter";
    case NodeType::BlendFilter:
      return "BlendFilter";
    case NodeType::ColorMatrixFilter:
      return "ColorMatrixFilter";
    case NodeType::Image:
      return "Image";
    case NodeType::PathData:
      return "PathData";
    case NodeType::Composition:
      return "Composition";
    case NodeType::Layer:
      return "Layer";
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
