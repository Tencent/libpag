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

#include <string>
#include "pagx/model/ColorSource.h"
#include "pagx/model/Element.h"
#include "pagx/model/Fill.h"
#include "pagx/model/ImagePattern.h"
#include "pagx/model/Layer.h"
#include "pagx/model/LayerFilter.h"
#include "pagx/model/LayerStyle.h"
#include "pagx/model/MergePath.h"
#include "pagx/model/Node.h"
#include "pagx/model/Polystar.h"
#include "pagx/model/RangeSelector.h"
#include "pagx/model/Repeater.h"
#include "pagx/model/Stroke.h"
#include "pagx/model/TextLayout.h"
#include "pagx/model/TextPath.h"
#include "pagx/model/TrimPath.h"
#include "pagx/model/types/BlendMode.h"
#include "pagx/model/types/Color.h"
#include "pagx/model/types/ColorSpace.h"
#include "pagx/model/types/LayerPlacement.h"
#include "pagx/model/types/TileMode.h"

namespace pagx {

//==============================================================================
// Node types
//==============================================================================
const char* NodeTypeName(NodeType type);

//==============================================================================
// ColorSource types
//==============================================================================
const char* ColorSourceTypeName(ColorSourceType type);

//==============================================================================
// LayerStyle types
//==============================================================================
const char* LayerStyleTypeName(LayerStyleType type);

//==============================================================================
// LayerFilter types
//==============================================================================
const char* LayerFilterTypeName(LayerFilterType type);

//==============================================================================
// BlendMode
//==============================================================================
std::string BlendModeToString(BlendMode mode);
BlendMode BlendModeFromString(const std::string& str);

//==============================================================================
// FillRule
//==============================================================================
std::string FillRuleToString(FillRule rule);
FillRule FillRuleFromString(const std::string& str);

//==============================================================================
// LineCap, LineJoin, StrokeAlign
//==============================================================================
std::string LineCapToString(LineCap cap);
LineCap LineCapFromString(const std::string& str);
std::string LineJoinToString(LineJoin join);
LineJoin LineJoinFromString(const std::string& str);
std::string StrokeAlignToString(StrokeAlign align);
StrokeAlign StrokeAlignFromString(const std::string& str);

//==============================================================================
// TileMode
//==============================================================================
std::string TileModeToString(TileMode mode);
TileMode TileModeFromString(const std::string& str);

//==============================================================================
// LayerPlacement
//==============================================================================
std::string LayerPlacementToString(LayerPlacement placement);
LayerPlacement LayerPlacementFromString(const std::string& str);

//==============================================================================
// ColorSpace
//==============================================================================
std::string ColorSpaceToString(ColorSpace space);
ColorSpace ColorSpaceFromString(const std::string& str);

//==============================================================================
// TrimType
//==============================================================================
std::string TrimTypeToString(TrimType type);
TrimType TrimTypeFromString(const std::string& str);

//==============================================================================
// MaskType
//==============================================================================
std::string MaskTypeToString(MaskType type);
MaskType MaskTypeFromString(const std::string& str);

//==============================================================================
// PolystarType
//==============================================================================
std::string PolystarTypeToString(PolystarType type);
PolystarType PolystarTypeFromString(const std::string& str);

//==============================================================================
// MergePathMode
//==============================================================================
std::string MergePathModeToString(MergePathMode mode);
MergePathMode MergePathModeFromString(const std::string& str);

//==============================================================================
// SamplingMode
//==============================================================================
std::string SamplingModeToString(SamplingMode mode);
SamplingMode SamplingModeFromString(const std::string& str);

//==============================================================================
// TextAlign, VerticalAlign, Overflow
//==============================================================================
std::string TextAlignToString(TextAlign align);
TextAlign TextAlignFromString(const std::string& str);
std::string VerticalAlignToString(VerticalAlign align);
VerticalAlign VerticalAlignFromString(const std::string& str);
std::string TextDirectionToString(TextDirection direction);
TextDirection TextDirectionFromString(const std::string& str);

//==============================================================================
// RepeaterOrder
//==============================================================================
std::string RepeaterOrderToString(RepeaterOrder order);
RepeaterOrder RepeaterOrderFromString(const std::string& str);

//==============================================================================
// Selector types
//==============================================================================
std::string SelectorUnitToString(SelectorUnit unit);
SelectorUnit SelectorUnitFromString(const std::string& str);
std::string SelectorShapeToString(SelectorShape shape);
SelectorShape SelectorShapeFromString(const std::string& str);
std::string SelectorModeToString(SelectorMode mode);
SelectorMode SelectorModeFromString(const std::string& str);

//==============================================================================
// Color
//==============================================================================
std::string ColorToHexString(const Color& color, bool withAlpha = false);

}  // namespace pagx
