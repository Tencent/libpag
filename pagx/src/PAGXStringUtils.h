/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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
#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/Element.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LayerFilter.h"
#include "pagx/nodes/LayerStyle.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/Node.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RangeSelector.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/TextLayout.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/nodes/BlendMode.h"
#include "pagx/nodes/Color.h"
#include "pagx/nodes/ColorSpace.h"
#include "pagx/nodes/LayerPlacement.h"
#include "pagx/nodes/FilterMode.h"
#include "pagx/nodes/MipmapMode.h"
#include "pagx/nodes/TileMode.h"

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
// FilterMode, MipmapMode
//==============================================================================
std::string FilterModeToString(FilterMode mode);
FilterMode FilterModeFromString(const std::string& str);
std::string MipmapModeToString(MipmapMode mode);
MipmapMode MipmapModeFromString(const std::string& str);

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
