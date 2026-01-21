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

// This file provides backward compatibility.
// New code should include pagx/model/Model.h directly.

#include "pagx/model/Model.h"

namespace pagx {

// Type aliases for backward compatibility (deprecated, use new names)
using PAGXNode = Node;
using ResourceNode = Resource;
using ColorSourceNode = ColorSource;
using VectorElementNode = VectorElement;
using LayerStyleNode = LayerStyle;
using LayerFilterNode = LayerFilter;

// Color source nodes
using ColorStopNode = ColorStop;
using SolidColorNode = SolidColor;
using LinearGradientNode = LinearGradient;
using RadialGradientNode = RadialGradient;
using ConicGradientNode = ConicGradient;
using DiamondGradientNode = DiamondGradient;
using ImagePatternNode = ImagePattern;

// Geometry nodes
using RectangleNode = Rectangle;
using EllipseNode = Ellipse;
using PolystarNode = Polystar;
using PathNode = Path;
using TextSpanNode = TextSpan;

// Painter nodes
using FillNode = Fill;
using StrokeNode = Stroke;

// Shape modifier nodes
using TrimPathNode = TrimPath;
using RoundCornerNode = RoundCorner;
using MergePathNode = MergePath;

// Text modifier nodes
using RangeSelectorNode = RangeSelector;
using TextModifierNode = TextModifier;
using TextPathNode = TextPath;
using TextLayoutNode = TextLayout;

// Other nodes
using RepeaterNode = Repeater;
using GroupNode = Group;

// Layer style nodes
using DropShadowStyleNode = DropShadowStyle;
using InnerShadowStyleNode = InnerShadowStyle;
using BackgroundBlurStyleNode = BackgroundBlurStyle;

// Layer filter nodes
using BlurFilterNode = BlurFilter;
using DropShadowFilterNode = DropShadowFilter;
using InnerShadowFilterNode = InnerShadowFilter;
using BlendFilterNode = BlendFilter;
using ColorMatrixFilterNode = ColorMatrixFilter;

// Resource nodes
using ImageNode = Image;
using PathDataNode = PathDataResource;
using CompositionNode = Composition;

// Layer
using LayerNode = Layer;

}  // namespace pagx
