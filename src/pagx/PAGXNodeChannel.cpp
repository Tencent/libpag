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

#include "pagx/PAGXNodeChannel.h"
#include <optional>
#include <unordered_map>
#include "base/utils/Log.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Gradient.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LayerStyle.h"
#include "pagx/nodes/LayoutNode.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/RangeSelector.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

// The access generators below turn a member pointer (and, for enums, the StringParser converters)
// into a uniform NodeAccessFn. A read copies the field into *getOut; a write validates the KeyValue
// alternative (or enum string) and copies into the field. Routing both directions through one
// generated function keeps reads and writes symmetric and removes the per-field if/else boilerplate.

template <typename T, auto Field>
static bool AccessFloat(Node* node, KeyValue* getOut, const KeyValue* setIn) {
  auto* self = static_cast<T*>(node);
  if (getOut != nullptr) {
    *getOut = self->*Field;
    return true;
  }
  const auto* value = std::get_if<float>(setIn);
  if (value == nullptr) {
    return false;
  }
  self->*Field = *value;
  return true;
}

template <typename T, auto Field>
static bool AccessBool(Node* node, KeyValue* getOut, const KeyValue* setIn) {
  auto* self = static_cast<T*>(node);
  if (getOut != nullptr) {
    *getOut = self->*Field;
    return true;
  }
  const auto* value = std::get_if<bool>(setIn);
  if (value == nullptr) {
    return false;
  }
  self->*Field = *value;
  return true;
}

template <typename T, auto Field>
static bool AccessInt(Node* node, KeyValue* getOut, const KeyValue* setIn) {
  auto* self = static_cast<T*>(node);
  if (getOut != nullptr) {
    *getOut = self->*Field;
    return true;
  }
  const auto* value = std::get_if<int>(setIn);
  if (value == nullptr) {
    return false;
  }
  self->*Field = *value;
  return true;
}

template <typename T, auto Field>
static bool AccessString(Node* node, KeyValue* getOut, const KeyValue* setIn) {
  auto* self = static_cast<T*>(node);
  if (getOut != nullptr) {
    *getOut = self->*Field;
    return true;
  }
  const auto* value = std::get_if<std::string>(setIn);
  if (value == nullptr) {
    return false;
  }
  self->*Field = *value;
  return true;
}

template <typename T, auto Field>
static bool AccessColor(Node* node, KeyValue* getOut, const KeyValue* setIn) {
  auto* self = static_cast<T*>(node);
  if (getOut != nullptr) {
    *getOut = self->*Field;
    return true;
  }
  const auto* value = std::get_if<Color>(setIn);
  if (value == nullptr) {
    return false;
  }
  self->*Field = *value;
  return true;
}

// Point sub-component access: addresses position.x / position.y etc. XAxis selects x vs y.
template <typename T, auto Field, bool XAxis>
static bool AccessPointAxis(Node* node, KeyValue* getOut, const KeyValue* setIn) {
  auto* self = static_cast<T*>(node);
  float& component = XAxis ? (self->*Field).x : (self->*Field).y;
  if (getOut != nullptr) {
    *getOut = component;
    return true;
  }
  const auto* value = std::get_if<float>(setIn);
  if (value == nullptr) {
    return false;
  }
  component = *value;
  return true;
}

// Size sub-component access: addresses size.width / size.height. WidthAxis selects width vs height.
template <typename T, auto Field, bool WidthAxis>
static bool AccessSizeAxis(Node* node, KeyValue* getOut, const KeyValue* setIn) {
  auto* self = static_cast<T*>(node);
  float& component = WidthAxis ? (self->*Field).width : (self->*Field).height;
  if (getOut != nullptr) {
    *getOut = component;
    return true;
  }
  const auto* value = std::get_if<float>(setIn);
  if (value == nullptr) {
    return false;
  }
  component = *value;
  return true;
}

// Padding sub-component access. Which: 0=left, 1=top, 2=right, 3=bottom.
template <typename T, auto Field, int Which>
static bool AccessPaddingComp(Node* node, KeyValue* getOut, const KeyValue* setIn) {
  auto* self = static_cast<T*>(node);
  Padding& padding = self->*Field;
  float* component =
      Which == 0 ? &padding.left
                 : (Which == 1 ? &padding.top : (Which == 2 ? &padding.right : &padding.bottom));
  if (getOut != nullptr) {
    *getOut = *component;
    return true;
  }
  const auto* value = std::get_if<float>(setIn);
  if (value == nullptr) {
    return false;
  }
  *component = *value;
  return true;
}

template <typename T, auto Field>
static bool AccessOptionalFloat(Node* node, KeyValue* getOut, const KeyValue* setIn) {
  auto* self = static_cast<T*>(node);
  if (getOut != nullptr) {
    if (!(self->*Field).has_value()) {
      return false;
    }
    *getOut = *(self->*Field);
    return true;
  }
  const auto* value = std::get_if<float>(setIn);
  if (value == nullptr) {
    return false;
  }
  self->*Field = *value;
  return true;
}

template <typename T, auto Field>
static bool AccessOptionalColor(Node* node, KeyValue* getOut, const KeyValue* setIn) {
  auto* self = static_cast<T*>(node);
  if (getOut != nullptr) {
    if (!(self->*Field).has_value()) {
      return false;
    }
    *getOut = *(self->*Field);
    return true;
  }
  const auto* value = std::get_if<Color>(setIn);
  if (value == nullptr) {
    return false;
  }
  self->*Field = *value;
  return true;
}

// Enum access: enums are carried as their string name (matching the PAGX XML attribute values).
// The to/from/isValid converters are the same StringParser functions used by importer and exporter.
template <typename T, typename E, auto Field, std::string (*ToString)(E),
          E (*FromString)(const std::string&), bool (*IsValid)(const std::string&)>
static bool AccessEnum(Node* node, KeyValue* getOut, const KeyValue* setIn) {
  auto* self = static_cast<T*>(node);
  if (getOut != nullptr) {
    *getOut = ToString(self->*Field);
    return true;
  }
  const auto* value = std::get_if<std::string>(setIn);
  if (value == nullptr || !IsValid(*value)) {
    return false;
  }
  self->*Field = FromString(*value);
  return true;
}

// Convenience macros that turn a (channel, member) pair into a NodeFieldDef row. They only build the
// table entries; all access logic lives in the templated generators above.
#define FIELD_FLOAT(T, name, member, cls) \
  { name, cls, &AccessFloat<T, &T::member> }
#define FIELD_BOOL(T, name, member, cls) \
  { name, cls, &AccessBool<T, &T::member> }
#define FIELD_INT(T, name, member, cls) \
  { name, cls, &AccessInt<T, &T::member> }
#define FIELD_STRING(T, name, member, cls) \
  { name, cls, &AccessString<T, &T::member> }
#define FIELD_COLOR(T, name, member, cls) \
  { name, cls, &AccessColor<T, &T::member> }
#define FIELD_POINT_X(T, name, member, cls) \
  { name, cls, &AccessPointAxis<T, &T::member, true> }
#define FIELD_POINT_Y(T, name, member, cls) \
  { name, cls, &AccessPointAxis<T, &T::member, false> }
#define FIELD_SIZE_W(T, name, member, cls) \
  { name, cls, &AccessSizeAxis<T, &T::member, true> }
#define FIELD_SIZE_H(T, name, member, cls) \
  { name, cls, &AccessSizeAxis<T, &T::member, false> }
#define FIELD_PADDING_L(T, name, member, cls) \
  { name, cls, &AccessPaddingComp<T, &T::member, 0> }
#define FIELD_PADDING_T(T, name, member, cls) \
  { name, cls, &AccessPaddingComp<T, &T::member, 1> }
#define FIELD_PADDING_R(T, name, member, cls) \
  { name, cls, &AccessPaddingComp<T, &T::member, 2> }
#define FIELD_PADDING_B(T, name, member, cls) \
  { name, cls, &AccessPaddingComp<T, &T::member, 3> }
#define FIELD_OPT_FLOAT(T, name, member, cls) \
  { name, cls, &AccessOptionalFloat<T, &T::member> }
#define FIELD_OPT_COLOR(T, name, member, cls) \
  { name, cls, &AccessOptionalColor<T, &T::member> }
#define FIELD_ENUM(T, name, member, cls, E) \
  { name, cls, &AccessEnum<T, E, &T::member, E##ToString, E##FromString, IsValid##E##String> }

// The shared LayoutNode constraint fields (layout inputs) appended to every LayoutNode-derived type.
// T must derive from LayoutNode so the member pointers resolve through the base subobject.
template <typename T>
static void AppendLayoutNodeFields(std::vector<NodeFieldDef>& table) {
  std::vector<NodeFieldDef> shared = {
      FIELD_FLOAT(T, "width", width, AnimClass::LayoutInput),
      FIELD_FLOAT(T, "height", height, AnimClass::LayoutInput),
      FIELD_FLOAT(T, "percentWidth", percentWidth, AnimClass::LayoutInput),
      FIELD_FLOAT(T, "percentHeight", percentHeight, AnimClass::LayoutInput),
      FIELD_FLOAT(T, "left", left, AnimClass::LayoutInput),
      FIELD_FLOAT(T, "right", right, AnimClass::LayoutInput),
      FIELD_FLOAT(T, "top", top, AnimClass::LayoutInput),
      FIELD_FLOAT(T, "bottom", bottom, AnimClass::LayoutInput),
      FIELD_FLOAT(T, "centerX", centerX, AnimClass::LayoutInput),
      FIELD_FLOAT(T, "centerY", centerY, AnimClass::LayoutInput),
  };
  table.insert(table.end(), shared.begin(), shared.end());
}

static std::vector<NodeFieldDef> BuildLayerFields() {
  std::vector<NodeFieldDef> table = {
      FIELD_STRING(Layer, "name", name, AnimClass::LayoutInput),
      FIELD_BOOL(Layer, "visible", visible, AnimClass::Animatable),
      FIELD_FLOAT(Layer, "alpha", alpha, AnimClass::Animatable),
      FIELD_ENUM(Layer, "blendMode", blendMode, AnimClass::Animatable, BlendMode),
      FIELD_FLOAT(Layer, "x", x, AnimClass::Animatable),
      FIELD_FLOAT(Layer, "y", y, AnimClass::Animatable),
      FIELD_BOOL(Layer, "preserve3D", preserve3D, AnimClass::Static),
      FIELD_BOOL(Layer, "antiAlias", antiAlias, AnimClass::Static),
      FIELD_BOOL(Layer, "groupOpacity", groupOpacity, AnimClass::Static),
      FIELD_BOOL(Layer, "passThroughBackground", passThroughBackground, AnimClass::Static),
      FIELD_BOOL(Layer, "clipToBounds", clipToBounds, AnimClass::LayoutInput),
      FIELD_ENUM(Layer, "maskType", maskType, AnimClass::Static, MaskType),
      FIELD_ENUM(Layer, "layout", layout, AnimClass::LayoutInput, LayoutMode),
      FIELD_FLOAT(Layer, "gap", gap, AnimClass::LayoutInput),
      FIELD_FLOAT(Layer, "flex", flex, AnimClass::LayoutInput),
      FIELD_ENUM(Layer, "alignment", alignment, AnimClass::LayoutInput, Alignment),
      FIELD_ENUM(Layer, "arrangement", arrangement, AnimClass::LayoutInput, Arrangement),
      FIELD_BOOL(Layer, "includeInLayout", includeInLayout, AnimClass::LayoutInput),
      FIELD_PADDING_L(Layer, "padding.left", padding, AnimClass::LayoutInput),
      FIELD_PADDING_T(Layer, "padding.top", padding, AnimClass::LayoutInput),
      FIELD_PADDING_R(Layer, "padding.right", padding, AnimClass::LayoutInput),
      FIELD_PADDING_B(Layer, "padding.bottom", padding, AnimClass::LayoutInput),
  };
  AppendLayoutNodeFields<Layer>(table);
  return table;
}

static std::vector<NodeFieldDef> BuildRectangleFields() {
  std::vector<NodeFieldDef> table = {
      FIELD_POINT_X(Rectangle, "position.x", position, AnimClass::Animatable),
      FIELD_POINT_Y(Rectangle, "position.y", position, AnimClass::Animatable),
      FIELD_SIZE_W(Rectangle, "size.width", size, AnimClass::Animatable),
      FIELD_SIZE_H(Rectangle, "size.height", size, AnimClass::Animatable),
      FIELD_FLOAT(Rectangle, "roundness", roundness, AnimClass::Animatable),
      FIELD_BOOL(Rectangle, "reversed", reversed, AnimClass::Static),
  };
  AppendLayoutNodeFields<Rectangle>(table);
  return table;
}

static std::vector<NodeFieldDef> BuildEllipseFields() {
  std::vector<NodeFieldDef> table = {
      FIELD_POINT_X(Ellipse, "position.x", position, AnimClass::Animatable),
      FIELD_POINT_Y(Ellipse, "position.y", position, AnimClass::Animatable),
      FIELD_SIZE_W(Ellipse, "size.width", size, AnimClass::Animatable),
      FIELD_SIZE_H(Ellipse, "size.height", size, AnimClass::Animatable),
      FIELD_BOOL(Ellipse, "reversed", reversed, AnimClass::Static),
  };
  AppendLayoutNodeFields<Ellipse>(table);
  return table;
}

static std::vector<NodeFieldDef> BuildPolystarFields() {
  std::vector<NodeFieldDef> table = {
      FIELD_POINT_X(Polystar, "position.x", position, AnimClass::Animatable),
      FIELD_POINT_Y(Polystar, "position.y", position, AnimClass::Animatable),
      FIELD_ENUM(Polystar, "type", type, AnimClass::Static, PolystarType),
      FIELD_FLOAT(Polystar, "pointCount", pointCount, AnimClass::Animatable),
      FIELD_FLOAT(Polystar, "outerRadius", outerRadius, AnimClass::Animatable),
      FIELD_FLOAT(Polystar, "innerRadius", innerRadius, AnimClass::Animatable),
      FIELD_FLOAT(Polystar, "rotation", rotation, AnimClass::Animatable),
      FIELD_FLOAT(Polystar, "outerRoundness", outerRoundness, AnimClass::Animatable),
      FIELD_FLOAT(Polystar, "innerRoundness", innerRoundness, AnimClass::Animatable),
      FIELD_BOOL(Polystar, "reversed", reversed, AnimClass::Static),
  };
  AppendLayoutNodeFields<Polystar>(table);
  return table;
}

static std::vector<NodeFieldDef> BuildPathFields() {
  std::vector<NodeFieldDef> table = {
      FIELD_POINT_X(Path, "position.x", position, AnimClass::Animatable),
      FIELD_POINT_Y(Path, "position.y", position, AnimClass::Animatable),
      FIELD_BOOL(Path, "reversed", reversed, AnimClass::Static),
  };
  AppendLayoutNodeFields<Path>(table);
  return table;
}

static std::vector<NodeFieldDef> BuildTextFields() {
  std::vector<NodeFieldDef> table = {
      FIELD_STRING(Text, "text", text, AnimClass::LayoutInput),
      FIELD_POINT_X(Text, "position.x", position, AnimClass::Animatable),
      FIELD_POINT_Y(Text, "position.y", position, AnimClass::Animatable),
      FIELD_STRING(Text, "fontFamily", fontFamily, AnimClass::LayoutInput),
      FIELD_STRING(Text, "fontStyle", fontStyle, AnimClass::LayoutInput),
      FIELD_FLOAT(Text, "fontSize", fontSize, AnimClass::LayoutInput),
      FIELD_FLOAT(Text, "letterSpacing", letterSpacing, AnimClass::LayoutInput),
      FIELD_BOOL(Text, "fauxBold", fauxBold, AnimClass::LayoutInput),
      FIELD_BOOL(Text, "fauxItalic", fauxItalic, AnimClass::LayoutInput),
      FIELD_ENUM(Text, "textAnchor", textAnchor, AnimClass::LayoutInput, TextAnchor),
      FIELD_ENUM(Text, "baseline", baseline, AnimClass::LayoutInput, TextBaseline),
  };
  AppendLayoutNodeFields<Text>(table);
  return table;
}

static std::vector<NodeFieldDef> BuildFillFields() {
  return {
      FIELD_FLOAT(Fill, "alpha", alpha, AnimClass::Animatable),
      FIELD_ENUM(Fill, "blendMode", blendMode, AnimClass::Static, BlendMode),
      FIELD_ENUM(Fill, "fillRule", fillRule, AnimClass::Static, FillRule),
      FIELD_ENUM(Fill, "placement", placement, AnimClass::Static, LayerPlacement),
  };
}

static std::vector<NodeFieldDef> BuildStrokeFields() {
  return {
      FIELD_FLOAT(Stroke, "width", width, AnimClass::Animatable),
      FIELD_FLOAT(Stroke, "alpha", alpha, AnimClass::Animatable),
      FIELD_ENUM(Stroke, "blendMode", blendMode, AnimClass::Static, BlendMode),
      FIELD_ENUM(Stroke, "cap", cap, AnimClass::Static, LineCap),
      FIELD_ENUM(Stroke, "join", join, AnimClass::Static, LineJoin),
      FIELD_FLOAT(Stroke, "miterLimit", miterLimit, AnimClass::Animatable),
      FIELD_FLOAT(Stroke, "dashOffset", dashOffset, AnimClass::Animatable),
      FIELD_BOOL(Stroke, "dashAdaptive", dashAdaptive, AnimClass::Static),
      FIELD_ENUM(Stroke, "align", align, AnimClass::Static, StrokeAlign),
      FIELD_ENUM(Stroke, "placement", placement, AnimClass::Static, LayerPlacement),
  };
}

static std::vector<NodeFieldDef> BuildTrimPathFields() {
  return {
      FIELD_FLOAT(TrimPath, "start", start, AnimClass::Animatable),
      FIELD_FLOAT(TrimPath, "end", end, AnimClass::Animatable),
      FIELD_FLOAT(TrimPath, "offset", offset, AnimClass::Animatable),
      FIELD_ENUM(TrimPath, "type", type, AnimClass::Static, TrimType),
  };
}

static std::vector<NodeFieldDef> BuildRoundCornerFields() {
  return {
      FIELD_FLOAT(RoundCorner, "radius", radius, AnimClass::Animatable),
  };
}

static std::vector<NodeFieldDef> BuildMergePathFields() {
  return {
      FIELD_ENUM(MergePath, "mode", mode, AnimClass::Static, MergePathMode),
  };
}

static std::vector<NodeFieldDef> BuildTextModifierFields() {
  return {
      FIELD_POINT_X(TextModifier, "anchor.x", anchor, AnimClass::Animatable),
      FIELD_POINT_Y(TextModifier, "anchor.y", anchor, AnimClass::Animatable),
      FIELD_POINT_X(TextModifier, "position.x", position, AnimClass::Animatable),
      FIELD_POINT_Y(TextModifier, "position.y", position, AnimClass::Animatable),
      FIELD_FLOAT(TextModifier, "rotation", rotation, AnimClass::Animatable),
      FIELD_POINT_X(TextModifier, "scale.x", scale, AnimClass::Animatable),
      FIELD_POINT_Y(TextModifier, "scale.y", scale, AnimClass::Animatable),
      FIELD_FLOAT(TextModifier, "skew", skew, AnimClass::Animatable),
      FIELD_FLOAT(TextModifier, "skewAxis", skewAxis, AnimClass::Animatable),
      FIELD_FLOAT(TextModifier, "alpha", alpha, AnimClass::Animatable),
      FIELD_OPT_COLOR(TextModifier, "fillColor", fillColor, AnimClass::Animatable),
      FIELD_OPT_COLOR(TextModifier, "strokeColor", strokeColor, AnimClass::Animatable),
      FIELD_OPT_FLOAT(TextModifier, "strokeWidth", strokeWidth, AnimClass::Animatable),
  };
}

static std::vector<NodeFieldDef> BuildTextPathFields() {
  std::vector<NodeFieldDef> table = {
      FIELD_POINT_X(TextPath, "baselineOrigin.x", baselineOrigin, AnimClass::LayoutInput),
      FIELD_POINT_Y(TextPath, "baselineOrigin.y", baselineOrigin, AnimClass::LayoutInput),
      FIELD_FLOAT(TextPath, "baselineAngle", baselineAngle, AnimClass::LayoutInput),
      FIELD_FLOAT(TextPath, "firstMargin", firstMargin, AnimClass::LayoutInput),
      FIELD_FLOAT(TextPath, "lastMargin", lastMargin, AnimClass::LayoutInput),
      FIELD_BOOL(TextPath, "perpendicular", perpendicular, AnimClass::LayoutInput),
      FIELD_BOOL(TextPath, "reversed", reversed, AnimClass::Static),
      FIELD_BOOL(TextPath, "forceAlignment", forceAlignment, AnimClass::LayoutInput),
  };
  AppendLayoutNodeFields<TextPath>(table);
  return table;
}

// Shared Group transform/layout fields, also used by TextBox which derives from Group.
template <typename T>
static void AppendGroupCommonFields(std::vector<NodeFieldDef>& table) {
  std::vector<NodeFieldDef> shared = {
      FIELD_POINT_X(T, "anchor.x", anchor, AnimClass::Animatable),
      FIELD_POINT_Y(T, "anchor.y", anchor, AnimClass::Animatable),
      FIELD_POINT_X(T, "position.x", position, AnimClass::Animatable),
      FIELD_POINT_Y(T, "position.y", position, AnimClass::Animatable),
      FIELD_FLOAT(T, "rotation", rotation, AnimClass::Animatable),
      FIELD_POINT_X(T, "scale.x", scale, AnimClass::Animatable),
      FIELD_POINT_Y(T, "scale.y", scale, AnimClass::Animatable),
      FIELD_FLOAT(T, "skew", skew, AnimClass::Animatable),
      FIELD_FLOAT(T, "skewAxis", skewAxis, AnimClass::Animatable),
      FIELD_FLOAT(T, "alpha", alpha, AnimClass::Animatable),
      FIELD_PADDING_L(T, "padding.left", padding, AnimClass::LayoutInput),
      FIELD_PADDING_T(T, "padding.top", padding, AnimClass::LayoutInput),
      FIELD_PADDING_R(T, "padding.right", padding, AnimClass::LayoutInput),
      FIELD_PADDING_B(T, "padding.bottom", padding, AnimClass::LayoutInput),
  };
  table.insert(table.end(), shared.begin(), shared.end());
  AppendLayoutNodeFields<T>(table);
}

static std::vector<NodeFieldDef> BuildGroupFields() {
  std::vector<NodeFieldDef> table = {};
  AppendGroupCommonFields<Group>(table);
  return table;
}

static std::vector<NodeFieldDef> BuildTextBoxFields() {
  std::vector<NodeFieldDef> table = {
      FIELD_ENUM(TextBox, "textAlign", textAlign, AnimClass::LayoutInput, TextAlign),
      FIELD_ENUM(TextBox, "paragraphAlign", paragraphAlign, AnimClass::LayoutInput, ParagraphAlign),
      FIELD_ENUM(TextBox, "writingMode", writingMode, AnimClass::LayoutInput, WritingMode),
      FIELD_FLOAT(TextBox, "lineHeight", lineHeight, AnimClass::LayoutInput),
      FIELD_BOOL(TextBox, "wordWrap", wordWrap, AnimClass::LayoutInput),
      FIELD_ENUM(TextBox, "overflow", overflow, AnimClass::LayoutInput, Overflow),
  };
  AppendGroupCommonFields<TextBox>(table);
  return table;
}

static std::vector<NodeFieldDef> BuildRepeaterFields() {
  return {
      FIELD_FLOAT(Repeater, "copies", copies, AnimClass::Animatable),
      FIELD_FLOAT(Repeater, "offset", offset, AnimClass::Animatable),
      FIELD_ENUM(Repeater, "order", order, AnimClass::Static, RepeaterOrder),
      FIELD_POINT_X(Repeater, "anchor.x", anchor, AnimClass::Animatable),
      FIELD_POINT_Y(Repeater, "anchor.y", anchor, AnimClass::Animatable),
      FIELD_POINT_X(Repeater, "position.x", position, AnimClass::Animatable),
      FIELD_POINT_Y(Repeater, "position.y", position, AnimClass::Animatable),
      FIELD_FLOAT(Repeater, "rotation", rotation, AnimClass::Animatable),
      FIELD_POINT_X(Repeater, "scale.x", scale, AnimClass::Animatable),
      FIELD_POINT_Y(Repeater, "scale.y", scale, AnimClass::Animatable),
      FIELD_FLOAT(Repeater, "startAlpha", startAlpha, AnimClass::Animatable),
      FIELD_FLOAT(Repeater, "endAlpha", endAlpha, AnimClass::Animatable),
  };
}

static std::vector<NodeFieldDef> BuildRangeSelectorFields() {
  return {
      FIELD_FLOAT(RangeSelector, "start", start, AnimClass::Animatable),
      FIELD_FLOAT(RangeSelector, "end", end, AnimClass::Animatable),
      FIELD_FLOAT(RangeSelector, "offset", offset, AnimClass::Animatable),
      FIELD_ENUM(RangeSelector, "unit", unit, AnimClass::Static, SelectorUnit),
      FIELD_ENUM(RangeSelector, "shape", shape, AnimClass::Static, SelectorShape),
      FIELD_FLOAT(RangeSelector, "easeIn", easeIn, AnimClass::Animatable),
      FIELD_FLOAT(RangeSelector, "easeOut", easeOut, AnimClass::Animatable),
      FIELD_ENUM(RangeSelector, "mode", mode, AnimClass::Static, SelectorMode),
      FIELD_FLOAT(RangeSelector, "weight", weight, AnimClass::Animatable),
      FIELD_BOOL(RangeSelector, "randomOrder", randomOrder, AnimClass::Static),
      FIELD_INT(RangeSelector, "randomSeed", randomSeed, AnimClass::Static),
  };
}

static std::vector<NodeFieldDef> BuildSolidColorFields() {
  return {
      FIELD_COLOR(SolidColor, "color", color, AnimClass::Animatable),
  };
}

static std::vector<NodeFieldDef> BuildLinearGradientFields() {
  return {
      FIELD_POINT_X(LinearGradient, "startPoint.x", startPoint, AnimClass::Animatable),
      FIELD_POINT_Y(LinearGradient, "startPoint.y", startPoint, AnimClass::Animatable),
      FIELD_POINT_X(LinearGradient, "endPoint.x", endPoint, AnimClass::Animatable),
      FIELD_POINT_Y(LinearGradient, "endPoint.y", endPoint, AnimClass::Animatable),
      FIELD_BOOL(LinearGradient, "fitsToGeometry", fitsToGeometry, AnimClass::Static),
  };
}

static std::vector<NodeFieldDef> BuildRadialGradientFields() {
  return {
      FIELD_POINT_X(RadialGradient, "center.x", center, AnimClass::Animatable),
      FIELD_POINT_Y(RadialGradient, "center.y", center, AnimClass::Animatable),
      FIELD_FLOAT(RadialGradient, "radius", radius, AnimClass::Animatable),
      FIELD_BOOL(RadialGradient, "fitsToGeometry", fitsToGeometry, AnimClass::Static),
  };
}

static std::vector<NodeFieldDef> BuildConicGradientFields() {
  return {
      FIELD_POINT_X(ConicGradient, "center.x", center, AnimClass::Animatable),
      FIELD_POINT_Y(ConicGradient, "center.y", center, AnimClass::Animatable),
      FIELD_FLOAT(ConicGradient, "startAngle", startAngle, AnimClass::Animatable),
      FIELD_FLOAT(ConicGradient, "endAngle", endAngle, AnimClass::Animatable),
      FIELD_BOOL(ConicGradient, "fitsToGeometry", fitsToGeometry, AnimClass::Static),
  };
}

static std::vector<NodeFieldDef> BuildDiamondGradientFields() {
  return {
      FIELD_POINT_X(DiamondGradient, "center.x", center, AnimClass::Animatable),
      FIELD_POINT_Y(DiamondGradient, "center.y", center, AnimClass::Animatable),
      FIELD_FLOAT(DiamondGradient, "radius", radius, AnimClass::Animatable),
      FIELD_BOOL(DiamondGradient, "fitsToGeometry", fitsToGeometry, AnimClass::Static),
  };
}

static std::vector<NodeFieldDef> BuildColorStopFields() {
  return {
      FIELD_FLOAT(ColorStop, "offset", offset, AnimClass::Animatable),
      FIELD_COLOR(ColorStop, "color", color, AnimClass::Animatable),
  };
}

static std::vector<NodeFieldDef> BuildImagePatternFields() {
  return {
      FIELD_ENUM(ImagePattern, "tileModeX", tileModeX, AnimClass::Static, TileMode),
      FIELD_ENUM(ImagePattern, "tileModeY", tileModeY, AnimClass::Static, TileMode),
      FIELD_ENUM(ImagePattern, "filterMode", filterMode, AnimClass::Static, FilterMode),
      FIELD_ENUM(ImagePattern, "mipmapMode", mipmapMode, AnimClass::Static, MipmapMode),
      FIELD_ENUM(ImagePattern, "scaleMode", scaleMode, AnimClass::Static, ScaleMode),
  };
}

static std::vector<NodeFieldDef> BuildDropShadowStyleFields() {
  return {
      FIELD_ENUM(DropShadowStyle, "blendMode", blendMode, AnimClass::Static, BlendMode),
      FIELD_BOOL(DropShadowStyle, "excludeChildEffects", excludeChildEffects, AnimClass::Static),
      FIELD_FLOAT(DropShadowStyle, "offsetX", offsetX, AnimClass::Animatable),
      FIELD_FLOAT(DropShadowStyle, "offsetY", offsetY, AnimClass::Animatable),
      FIELD_FLOAT(DropShadowStyle, "blurX", blurX, AnimClass::Animatable),
      FIELD_FLOAT(DropShadowStyle, "blurY", blurY, AnimClass::Animatable),
      FIELD_COLOR(DropShadowStyle, "color", color, AnimClass::Animatable),
      FIELD_BOOL(DropShadowStyle, "showBehindLayer", showBehindLayer, AnimClass::Animatable),
  };
}

static std::vector<NodeFieldDef> BuildInnerShadowStyleFields() {
  return {
      FIELD_ENUM(InnerShadowStyle, "blendMode", blendMode, AnimClass::Static, BlendMode),
      FIELD_BOOL(InnerShadowStyle, "excludeChildEffects", excludeChildEffects, AnimClass::Static),
      FIELD_FLOAT(InnerShadowStyle, "offsetX", offsetX, AnimClass::Animatable),
      FIELD_FLOAT(InnerShadowStyle, "offsetY", offsetY, AnimClass::Animatable),
      FIELD_FLOAT(InnerShadowStyle, "blurX", blurX, AnimClass::Animatable),
      FIELD_FLOAT(InnerShadowStyle, "blurY", blurY, AnimClass::Animatable),
      FIELD_COLOR(InnerShadowStyle, "color", color, AnimClass::Animatable),
  };
}

static std::vector<NodeFieldDef> BuildBackgroundBlurStyleFields() {
  return {
      FIELD_ENUM(BackgroundBlurStyle, "blendMode", blendMode, AnimClass::Static, BlendMode),
      FIELD_BOOL(BackgroundBlurStyle, "excludeChildEffects", excludeChildEffects,
                 AnimClass::Static),
      FIELD_FLOAT(BackgroundBlurStyle, "blurX", blurX, AnimClass::Animatable),
      FIELD_FLOAT(BackgroundBlurStyle, "blurY", blurY, AnimClass::Animatable),
      FIELD_ENUM(BackgroundBlurStyle, "tileMode", tileMode, AnimClass::Static, TileMode),
  };
}

static std::vector<NodeFieldDef> BuildBlurFilterFields() {
  return {
      FIELD_FLOAT(BlurFilter, "blurX", blurX, AnimClass::Animatable),
      FIELD_FLOAT(BlurFilter, "blurY", blurY, AnimClass::Animatable),
      FIELD_ENUM(BlurFilter, "tileMode", tileMode, AnimClass::Static, TileMode),
  };
}

static std::vector<NodeFieldDef> BuildDropShadowFilterFields() {
  return {
      FIELD_FLOAT(DropShadowFilter, "offsetX", offsetX, AnimClass::Animatable),
      FIELD_FLOAT(DropShadowFilter, "offsetY", offsetY, AnimClass::Animatable),
      FIELD_FLOAT(DropShadowFilter, "blurX", blurX, AnimClass::Animatable),
      FIELD_FLOAT(DropShadowFilter, "blurY", blurY, AnimClass::Animatable),
      FIELD_COLOR(DropShadowFilter, "color", color, AnimClass::Animatable),
      FIELD_BOOL(DropShadowFilter, "shadowOnly", shadowOnly, AnimClass::Animatable),
  };
}

static std::vector<NodeFieldDef> BuildInnerShadowFilterFields() {
  return {
      FIELD_FLOAT(InnerShadowFilter, "offsetX", offsetX, AnimClass::Animatable),
      FIELD_FLOAT(InnerShadowFilter, "offsetY", offsetY, AnimClass::Animatable),
      FIELD_FLOAT(InnerShadowFilter, "blurX", blurX, AnimClass::Animatable),
      FIELD_FLOAT(InnerShadowFilter, "blurY", blurY, AnimClass::Animatable),
      FIELD_COLOR(InnerShadowFilter, "color", color, AnimClass::Animatable),
      FIELD_BOOL(InnerShadowFilter, "shadowOnly", shadowOnly, AnimClass::Animatable),
  };
}

static std::vector<NodeFieldDef> BuildBlendFilterFields() {
  return {
      FIELD_COLOR(BlendFilter, "color", color, AnimClass::Animatable),
      FIELD_ENUM(BlendFilter, "blendMode", blendMode, AnimClass::Static, BlendMode),
  };
}

const std::vector<NodeFieldDef>& NodeFieldsFor(NodeType type) {
  static const std::vector<NodeFieldDef> empty = {};
  // Each table is built once on first use. Static locals keep the member-pointer-generated access
  // functions and channel rows alive for the process lifetime.
  switch (type) {
    case NodeType::Layer: {
      static const std::vector<NodeFieldDef> table = BuildLayerFields();
      return table;
    }
    case NodeType::Rectangle: {
      static const std::vector<NodeFieldDef> table = BuildRectangleFields();
      return table;
    }
    case NodeType::Ellipse: {
      static const std::vector<NodeFieldDef> table = BuildEllipseFields();
      return table;
    }
    case NodeType::Polystar: {
      static const std::vector<NodeFieldDef> table = BuildPolystarFields();
      return table;
    }
    case NodeType::Path: {
      static const std::vector<NodeFieldDef> table = BuildPathFields();
      return table;
    }
    case NodeType::Text: {
      static const std::vector<NodeFieldDef> table = BuildTextFields();
      return table;
    }
    case NodeType::Fill: {
      static const std::vector<NodeFieldDef> table = BuildFillFields();
      return table;
    }
    case NodeType::Stroke: {
      static const std::vector<NodeFieldDef> table = BuildStrokeFields();
      return table;
    }
    case NodeType::TrimPath: {
      static const std::vector<NodeFieldDef> table = BuildTrimPathFields();
      return table;
    }
    case NodeType::RoundCorner: {
      static const std::vector<NodeFieldDef> table = BuildRoundCornerFields();
      return table;
    }
    case NodeType::MergePath: {
      static const std::vector<NodeFieldDef> table = BuildMergePathFields();
      return table;
    }
    case NodeType::TextModifier: {
      static const std::vector<NodeFieldDef> table = BuildTextModifierFields();
      return table;
    }
    case NodeType::TextPath: {
      static const std::vector<NodeFieldDef> table = BuildTextPathFields();
      return table;
    }
    case NodeType::TextBox: {
      static const std::vector<NodeFieldDef> table = BuildTextBoxFields();
      return table;
    }
    case NodeType::Group: {
      static const std::vector<NodeFieldDef> table = BuildGroupFields();
      return table;
    }
    case NodeType::Repeater: {
      static const std::vector<NodeFieldDef> table = BuildRepeaterFields();
      return table;
    }
    case NodeType::RangeSelector: {
      static const std::vector<NodeFieldDef> table = BuildRangeSelectorFields();
      return table;
    }
    case NodeType::SolidColor: {
      static const std::vector<NodeFieldDef> table = BuildSolidColorFields();
      return table;
    }
    case NodeType::LinearGradient: {
      static const std::vector<NodeFieldDef> table = BuildLinearGradientFields();
      return table;
    }
    case NodeType::RadialGradient: {
      static const std::vector<NodeFieldDef> table = BuildRadialGradientFields();
      return table;
    }
    case NodeType::ConicGradient: {
      static const std::vector<NodeFieldDef> table = BuildConicGradientFields();
      return table;
    }
    case NodeType::DiamondGradient: {
      static const std::vector<NodeFieldDef> table = BuildDiamondGradientFields();
      return table;
    }
    case NodeType::ColorStop: {
      static const std::vector<NodeFieldDef> table = BuildColorStopFields();
      return table;
    }
    case NodeType::ImagePattern: {
      static const std::vector<NodeFieldDef> table = BuildImagePatternFields();
      return table;
    }
    case NodeType::DropShadowStyle: {
      static const std::vector<NodeFieldDef> table = BuildDropShadowStyleFields();
      return table;
    }
    case NodeType::InnerShadowStyle: {
      static const std::vector<NodeFieldDef> table = BuildInnerShadowStyleFields();
      return table;
    }
    case NodeType::BackgroundBlurStyle: {
      static const std::vector<NodeFieldDef> table = BuildBackgroundBlurStyleFields();
      return table;
    }
    case NodeType::BlurFilter: {
      static const std::vector<NodeFieldDef> table = BuildBlurFilterFields();
      return table;
    }
    case NodeType::DropShadowFilter: {
      static const std::vector<NodeFieldDef> table = BuildDropShadowFilterFields();
      return table;
    }
    case NodeType::InnerShadowFilter: {
      static const std::vector<NodeFieldDef> table = BuildInnerShadowFilterFields();
      return table;
    }
    case NodeType::BlendFilter: {
      static const std::vector<NodeFieldDef> table = BuildBlendFilterFields();
      return table;
    }
    default:
      return empty;
  }
}

static const NodeFieldDef* FindField(NodeType type, const std::string& channel) {
  const auto& table = NodeFieldsFor(type);
  for (const auto& field : table) {
    if (channel == field.channel) {
      return &field;
    }
  }
  return nullptr;
}

bool GetNodeChannel(const Node* node, const std::string& channel, KeyValue* out) {
  if (node == nullptr || out == nullptr) {
    return false;
  }
  const auto* field = FindField(node->nodeType(), channel);
  if (field == nullptr) {
    return false;
  }
  // The read path never mutates the node; const_cast is safe because access only writes when setIn
  // is non-null, which it is not here.
  return field->access(const_cast<Node*>(node), out, nullptr);
}

bool SetNodeChannel(Node* node, const std::string& channel, const KeyValue& value) {
  if (node == nullptr) {
    return false;
  }
  const auto* field = FindField(node->nodeType(), channel);
  if (field == nullptr || !field->access(node, nullptr, &value)) {
    LOGE("SetNodeChannel: unhandled channel '%s' or value type mismatch for node type %d.",
         channel.c_str(), static_cast<int>(node->nodeType()));
    return false;
  }
  return true;
}

bool IsAnimatableChannel(NodeType type, const std::string& channel) {
  const auto* field = FindField(type, channel);
  return field != nullptr && field->animClass == AnimClass::Animatable;
}

}  // namespace pagx
