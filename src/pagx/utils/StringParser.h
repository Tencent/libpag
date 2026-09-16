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
#include <vector>
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/Node.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RangeSelector.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/types/Alignment.h"
#include "pagx/types/Arrangement.h"
#include "pagx/types/BlendMode.h"
#include "pagx/types/Color.h"
#include "pagx/types/ColorSpace.h"
#include "pagx/types/FilterMode.h"
#include "pagx/types/LayerPlacement.h"
#include "pagx/types/LayoutMode.h"
#include "pagx/types/Matrix.h"
#include "pagx/types/MipmapMode.h"
#include "pagx/types/NoiseMode.h"
#include "pagx/types/Padding.h"
#include "pagx/types/ScaleMode.h"
#include "pagx/types/TextAnchor.h"
#include "pagx/types/TextBaseline.h"
#include "pagx/types/TileMode.h"

namespace pagx {

//==============================================================================
// Node types
//==============================================================================
const char* NodeTypeName(NodeType type);

//==============================================================================
// BlendMode
//==============================================================================
std::string BlendModeToString(BlendMode mode);
BlendMode BlendModeFromString(const std::string& str);
bool IsValidBlendModeString(const std::string& str);

//==============================================================================
// FillRule
//==============================================================================
std::string FillRuleToString(FillRule rule);
FillRule FillRuleFromString(const std::string& str);
bool IsValidFillRuleString(const std::string& str);

//==============================================================================
// LineCap, LineJoin, StrokeAlign
//==============================================================================
std::string LineCapToString(LineCap cap);
LineCap LineCapFromString(const std::string& str);
bool IsValidLineCapString(const std::string& str);
std::string LineJoinToString(LineJoin join);
LineJoin LineJoinFromString(const std::string& str);
bool IsValidLineJoinString(const std::string& str);
std::string StrokeAlignToString(StrokeAlign align);
StrokeAlign StrokeAlignFromString(const std::string& str);
bool IsValidStrokeAlignString(const std::string& str);

//==============================================================================
// TileMode
//==============================================================================
std::string TileModeToString(TileMode mode);
TileMode TileModeFromString(const std::string& str);
bool IsValidTileModeString(const std::string& str);

//==============================================================================
// LayerPlacement
//==============================================================================
std::string LayerPlacementToString(LayerPlacement placement);
LayerPlacement LayerPlacementFromString(const std::string& str);
bool IsValidLayerPlacementString(const std::string& str);

//==============================================================================
// ColorSpace
//==============================================================================
std::string ColorSpaceToString(ColorSpace space);
ColorSpace ColorSpaceFromString(const std::string& str);
bool IsValidColorSpaceString(const std::string& str);

//==============================================================================
// TrimType
//==============================================================================
std::string TrimTypeToString(TrimType type);
TrimType TrimTypeFromString(const std::string& str);
bool IsValidTrimTypeString(const std::string& str);

//==============================================================================
// MaskType
//==============================================================================
std::string MaskTypeToString(MaskType type);
MaskType MaskTypeFromString(const std::string& str);
bool IsValidMaskTypeString(const std::string& str);

//==============================================================================
// PolystarType
//==============================================================================
std::string PolystarTypeToString(PolystarType type);
PolystarType PolystarTypeFromString(const std::string& str);
bool IsValidPolystarTypeString(const std::string& str);

//==============================================================================
// MergePathMode
//==============================================================================
std::string MergePathModeToString(MergePathMode mode);
MergePathMode MergePathModeFromString(const std::string& str);
bool IsValidMergePathModeString(const std::string& str);

//==============================================================================
// FilterMode, MipmapMode
//==============================================================================
std::string FilterModeToString(FilterMode mode);
FilterMode FilterModeFromString(const std::string& str);
bool IsValidFilterModeString(const std::string& str);
std::string MipmapModeToString(MipmapMode mode);
MipmapMode MipmapModeFromString(const std::string& str);
bool IsValidMipmapModeString(const std::string& str);

//==============================================================================
// ScaleMode
//==============================================================================
std::string ScaleModeToString(ScaleMode mode);
ScaleMode ScaleModeFromString(const std::string& str);
bool IsValidScaleModeString(const std::string& str);

//==============================================================================
// TextAlign, ParagraphAlign, TextAnchor, WritingMode, Overflow
//==============================================================================
std::string TextAlignToString(TextAlign align);
TextAlign TextAlignFromString(const std::string& str);
bool IsValidTextAlignString(const std::string& str);
std::string ParagraphAlignToString(ParagraphAlign align);
ParagraphAlign ParagraphAlignFromString(const std::string& str);
bool IsValidParagraphAlignString(const std::string& str);
std::string TextAnchorToString(TextAnchor anchor);
TextAnchor TextAnchorFromString(const std::string& str);
bool IsValidTextAnchorString(const std::string& str);
std::string TextBaselineToString(TextBaseline baseline);
TextBaseline TextBaselineFromString(const std::string& str);
bool IsValidTextBaselineString(const std::string& str);
std::string WritingModeToString(WritingMode mode);
WritingMode WritingModeFromString(const std::string& str);
bool IsValidWritingModeString(const std::string& str);
std::string OverflowToString(Overflow value);
Overflow OverflowFromString(const std::string& str);
bool IsValidOverflowString(const std::string& str);

//==============================================================================
// RepeaterOrder
//==============================================================================
std::string RepeaterOrderToString(RepeaterOrder order);
RepeaterOrder RepeaterOrderFromString(const std::string& str);
bool IsValidRepeaterOrderString(const std::string& str);

//==============================================================================
// LayoutMode
//==============================================================================
std::string LayoutModeToString(LayoutMode mode);
LayoutMode LayoutModeFromString(const std::string& str);
bool IsValidLayoutModeString(const std::string& str);

//==============================================================================
// Alignment
//==============================================================================
std::string AlignmentToString(Alignment align);
Alignment AlignmentFromString(const std::string& str);
bool IsValidAlignmentString(const std::string& str);

//==============================================================================
// Arrangement
//==============================================================================
std::string ArrangementToString(Arrangement arr);
Arrangement ArrangementFromString(const std::string& str);
bool IsValidArrangementString(const std::string& str);

//==============================================================================
// NoiseMode
//==============================================================================
std::string NoiseModeToString(NoiseMode mode);
NoiseMode NoiseModeFromString(const std::string& str);
bool IsValidNoiseModeString(const std::string& str);

//==============================================================================
// Padding
//==============================================================================
Padding PaddingFromString(const std::string& str);
std::string PaddingToString(const Padding& padding);

//==============================================================================
// Selector types
//==============================================================================
std::string SelectorUnitToString(SelectorUnit unit);
SelectorUnit SelectorUnitFromString(const std::string& str);
bool IsValidSelectorUnitString(const std::string& str);
std::string SelectorShapeToString(SelectorShape shape);
SelectorShape SelectorShapeFromString(const std::string& str);
bool IsValidSelectorShapeString(const std::string& str);
std::string SelectorModeToString(SelectorMode mode);
SelectorMode SelectorModeFromString(const std::string& str);
bool IsValidSelectorModeString(const std::string& str);

//==============================================================================
// Color
//==============================================================================
std::string ColorToHexString(const Color& color, bool withAlpha = false);

//==============================================================================
// Matrix
//==============================================================================
std::string MatrixToString(const Matrix& matrix);
Matrix MatrixFromString(const std::string& str);

//==============================================================================
// String parsing utilities
//==============================================================================
std::vector<float> ParseFloatList(const std::string& str);
std::string FloatToString(float value);

/**
 * Parses a CSS `hsl()` / `hsla()` color string per CSS Color Module Level 4. Supports both
 * the legacy comma syntax (`hsl(120, 100%, 50%)`, `hsla(120, 100%, 50%, 0.5)`) and the modern
 * space-separated form (`hsl(120 100% 50%)`, `hsl(120 100% 50% / 50%)`). Hue accepts `deg`,
 * `rad`, `grad`, `turn`, or a unitless number (interpreted as degrees). Saturation / lightness
 * accept a percentage with `%` or a unitless number on a 0..100 scale. Alpha is unitless in
 * [0, 1] or a percentage in [0%, 100%]. Returns false on malformed input and leaves `out`
 * untouched. Function head matching is ASCII case-insensitive.
 */
bool ParseCSSHSLColor(const std::string& value, Color& out);

/**
 * Parses a CSS Color Module Level 4 `color()` functional notation such as
 * `color(srgb 0.15 0.88 0.81)` or `color(srgb 0.15 0.88 0.81 / 0.6)`. Chrome emits this form
 * from `getComputedStyle` whenever the authored value mixes color spaces or uses CSS color
 * functions even when the inputs are plain `rgb()`/`rgba()`. Channels are 0..1 floats; a `%`
 * suffix is also accepted (mapped onto 0..1). Alpha follows the `/` separator and may be a
 * unitless number in [0, 1] or a percentage. Only the `srgb` color space is honoured for now;
 * other spaces (display-p3, rec2020, …) parse but are downgraded to sRGB by returning false so
 * callers can emit a precise diagnostic. Returns false on malformed input or an unsupported
 * color space and leaves `out` untouched. Function head matching is ASCII case-insensitive.
 */
bool ParseCSSColorFunction(const std::string& value, Color& out);

/**
 * Formats a float as a pixel coordinate or size with at most two decimal places. Unlike
 * FloatToString (which uses %g and preserves full float precision), this trims sub-pixel
 * noise that only adds visual clutter to HTML/CSS output. Use this for left/top/width/height
 * /translate/line-height-style numbers. Do not use for SVG path data, transform matrices,
 * color channels, or other precision-sensitive values.
 */
std::string CoordToString(float value);

/**
 * Formats a float for CSS/HTML output using at most three decimal places, with trailing
 * zeros and a bare trailing dot stripped. Unlike FloatToString (which uses %g and prints
 * up to 6 significant digits, producing inconsistent precision such as 0.0823529 next to
 * -1.90915 next to -11.31), this gives uniform fractional precision suitable for
 * transform matrix coefficients, rotation degrees, scale factors, alpha values, gradient
 * stop percentages, SVG path data, and other CSS numerics. Px coordinates should still
 * use CoordToString (two decimal places). PAGX serialization and SVG export keep
 * FloatToString to preserve full roundtrip precision.
 */
std::string CssFloatToString(float value);

//==============================================================================
// Custom data key validation
//==============================================================================
bool IsValidCustomDataKey(const std::string& key);

}  // namespace pagx
