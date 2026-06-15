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
#include <string_view>
#include <unordered_map>
#include "base/utils/Log.h"
#include "pagx/PAGXChannelTable.h"
#include "pagx/PAGXDefaults.h"
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

// Short aliases for the channel flag combinations used by the tables below. Anim: has a runtime
// writer (animatable in place). Layout: editing it on the document needs a layout pass to show up.
// AnimLayout: both (e.g. a shape's size, which is animated in place but, when edited, must re-run
// layout because the rendered size is read through a layout-derived accessor). NoFlags: neither.
static constexpr ChannelFlags Anim = ChannelFlags::Animatable;
static constexpr ChannelFlags Layout = ChannelFlags::RequiresLayout;
static constexpr ChannelFlags AnimLayout = ChannelFlags::Animatable | ChannelFlags::RequiresLayout;
static constexpr ChannelFlags NoFlags = ChannelFlags::None;

// The access generators below turn a member pointer (and, for enums, the StringParser converters)
// into a uniform ChannelAccessor. A read copies the field into *getOut; a write validates the KeyValue
// alternative (or enum string) and copies into the field; a reset (both getOut and setIn null) copies
// the corresponding field of a default-constructed node of type T back into the field. Routing all
// three directions through one generated function keeps them symmetric and removes the per-field
// if/else boilerplate.

template <typename T, auto Field>
static bool AccessFloat(Node* node, KeyValue* getOut, const KeyValue* setIn) {
  auto* self = static_cast<T*>(node);
  if (getOut == nullptr && setIn == nullptr) {
    self->*Field = Default<T>().*Field;
    return true;
  }
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
  if (getOut == nullptr && setIn == nullptr) {
    self->*Field = Default<T>().*Field;
    return true;
  }
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
  if (getOut == nullptr && setIn == nullptr) {
    self->*Field = Default<T>().*Field;
    return true;
  }
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
  if (getOut == nullptr && setIn == nullptr) {
    self->*Field = Default<T>().*Field;
    return true;
  }
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
  if (getOut == nullptr && setIn == nullptr) {
    self->*Field = Default<T>().*Field;
    return true;
  }
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
  if (getOut == nullptr && setIn == nullptr) {
    const auto& def = Default<T>().*Field;
    component = XAxis ? def.x : def.y;
    return true;
  }
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
  if (getOut == nullptr && setIn == nullptr) {
    const auto& def = Default<T>().*Field;
    component = WidthAxis ? def.width : def.height;
    return true;
  }
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
  if (getOut == nullptr && setIn == nullptr) {
    const Padding& def = Default<T>().*Field;
    *component =
        Which == 0 ? def.left : (Which == 1 ? def.top : (Which == 2 ? def.right : def.bottom));
    return true;
  }
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
  if (getOut == nullptr && setIn == nullptr) {
    self->*Field = Default<T>().*Field;
    return true;
  }
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
  if (getOut == nullptr && setIn == nullptr) {
    self->*Field = Default<T>().*Field;
    return true;
  }
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
  if (getOut == nullptr && setIn == nullptr) {
    self->*Field = Default<T>().*Field;
    return true;
  }
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

// Convenience macros that turn a (channel, member) pair into a ChannelDef row. They only build the
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
static void AppendLayoutNodeFields(std::vector<ChannelDef>& table) {
  std::vector<ChannelDef> shared = {
      FIELD_FLOAT(T, "width", width, Layout),
      FIELD_FLOAT(T, "height", height, Layout),
      FIELD_FLOAT(T, "percentWidth", percentWidth, Layout),
      FIELD_FLOAT(T, "percentHeight", percentHeight, Layout),
      FIELD_FLOAT(T, "left", left, Layout),
      FIELD_FLOAT(T, "right", right, Layout),
      FIELD_FLOAT(T, "top", top, Layout),
      FIELD_FLOAT(T, "bottom", bottom, Layout),
      FIELD_FLOAT(T, "centerX", centerX, Layout),
      FIELD_FLOAT(T, "centerY", centerY, Layout),
  };
  table.insert(table.end(), shared.begin(), shared.end());
}

static std::vector<ChannelDef> BuildLayerFields() {
  std::vector<ChannelDef> table = {
      FIELD_STRING(Layer, "name", name, NoFlags),
      FIELD_BOOL(Layer, "visible", visible, Anim),
      FIELD_FLOAT(Layer, "alpha", alpha, Anim),
      FIELD_ENUM(Layer, "blendMode", blendMode, Anim, BlendMode),
      FIELD_FLOAT(Layer, "x", x, AnimLayout),
      FIELD_FLOAT(Layer, "y", y, AnimLayout),
      FIELD_BOOL(Layer, "preserve3D", preserve3D, NoFlags),
      FIELD_BOOL(Layer, "antiAlias", antiAlias, NoFlags),
      FIELD_BOOL(Layer, "groupOpacity", groupOpacity, NoFlags),
      FIELD_BOOL(Layer, "passThroughBackground", passThroughBackground, NoFlags),
      FIELD_BOOL(Layer, "clipToBounds", clipToBounds, Layout),
      FIELD_ENUM(Layer, "maskType", maskType, NoFlags, MaskType),
      FIELD_ENUM(Layer, "layout", layout, Layout, LayoutMode),
      FIELD_FLOAT(Layer, "gap", gap, Layout),
      FIELD_FLOAT(Layer, "flex", flex, Layout),
      FIELD_ENUM(Layer, "alignment", alignment, Layout, Alignment),
      FIELD_ENUM(Layer, "arrangement", arrangement, Layout, Arrangement),
      FIELD_BOOL(Layer, "includeInLayout", includeInLayout, Layout),
      FIELD_PADDING_L(Layer, "padding.left", padding, Layout),
      FIELD_PADDING_T(Layer, "padding.top", padding, Layout),
      FIELD_PADDING_R(Layer, "padding.right", padding, Layout),
      FIELD_PADDING_B(Layer, "padding.bottom", padding, Layout),
  };
  AppendLayoutNodeFields<Layer>(table);
  return table;
}

static std::vector<ChannelDef> BuildRectangleFields() {
  std::vector<ChannelDef> table = {
      FIELD_POINT_X(Rectangle, "position.x", position, AnimLayout),
      FIELD_POINT_Y(Rectangle, "position.y", position, AnimLayout),
      FIELD_SIZE_W(Rectangle, "size.width", size, AnimLayout),
      FIELD_SIZE_H(Rectangle, "size.height", size, AnimLayout),
      FIELD_FLOAT(Rectangle, "roundness", roundness, Anim),
      FIELD_BOOL(Rectangle, "reversed", reversed, NoFlags),
  };
  AppendLayoutNodeFields<Rectangle>(table);
  return table;
}

static std::vector<ChannelDef> BuildEllipseFields() {
  std::vector<ChannelDef> table = {
      FIELD_POINT_X(Ellipse, "position.x", position, AnimLayout),
      FIELD_POINT_Y(Ellipse, "position.y", position, AnimLayout),
      FIELD_SIZE_W(Ellipse, "size.width", size, AnimLayout),
      FIELD_SIZE_H(Ellipse, "size.height", size, AnimLayout),
      FIELD_BOOL(Ellipse, "reversed", reversed, NoFlags),
  };
  AppendLayoutNodeFields<Ellipse>(table);
  return table;
}

static std::vector<ChannelDef> BuildPolystarFields() {
  std::vector<ChannelDef> table = {
      FIELD_POINT_X(Polystar, "position.x", position, AnimLayout),
      FIELD_POINT_Y(Polystar, "position.y", position, AnimLayout),
      FIELD_ENUM(Polystar, "type", type, Layout, PolystarType),
      FIELD_FLOAT(Polystar, "pointCount", pointCount, AnimLayout),
      FIELD_FLOAT(Polystar, "outerRadius", outerRadius, AnimLayout),
      FIELD_FLOAT(Polystar, "innerRadius", innerRadius, AnimLayout),
      FIELD_FLOAT(Polystar, "rotation", rotation, AnimLayout),
      FIELD_FLOAT(Polystar, "outerRoundness", outerRoundness, Anim),
      FIELD_FLOAT(Polystar, "innerRoundness", innerRoundness, Anim),
      FIELD_BOOL(Polystar, "reversed", reversed, NoFlags),
  };
  AppendLayoutNodeFields<Polystar>(table);
  return table;
}

static std::vector<ChannelDef> BuildPathFields() {
  std::vector<ChannelDef> table = {
      FIELD_POINT_X(Path, "position.x", position, Layout),
      FIELD_POINT_Y(Path, "position.y", position, Layout),
      FIELD_BOOL(Path, "reversed", reversed, NoFlags),
  };
  AppendLayoutNodeFields<Path>(table);
  return table;
}

static std::vector<ChannelDef> BuildTextFields() {
  std::vector<ChannelDef> table = {
      FIELD_STRING(Text, "text", text, Layout),
      FIELD_POINT_X(Text, "x", position, AnimLayout),
      FIELD_POINT_Y(Text, "y", position, AnimLayout),
      FIELD_STRING(Text, "fontFamily", fontFamily, Layout),
      FIELD_STRING(Text, "fontStyle", fontStyle, Layout),
      FIELD_FLOAT(Text, "fontSize", fontSize, Layout),
      FIELD_FLOAT(Text, "letterSpacing", letterSpacing, Layout),
      FIELD_BOOL(Text, "fauxBold", fauxBold, Layout),
      FIELD_BOOL(Text, "fauxItalic", fauxItalic, Layout),
      FIELD_ENUM(Text, "textAnchor", textAnchor, Layout, TextAnchor),
      FIELD_ENUM(Text, "baseline", baseline, Layout, TextBaseline),
  };
  AppendLayoutNodeFields<Text>(table);
  return table;
}

static std::vector<ChannelDef> BuildFillFields() {
  return {
      FIELD_FLOAT(Fill, "alpha", alpha, Anim),
      FIELD_ENUM(Fill, "blendMode", blendMode, NoFlags, BlendMode),
      FIELD_ENUM(Fill, "fillRule", fillRule, NoFlags, FillRule),
      FIELD_ENUM(Fill, "placement", placement, NoFlags, LayerPlacement),
  };
}

static std::vector<ChannelDef> BuildStrokeFields() {
  return {
      FIELD_FLOAT(Stroke, "width", width, Anim),
      FIELD_FLOAT(Stroke, "alpha", alpha, Anim),
      FIELD_ENUM(Stroke, "blendMode", blendMode, NoFlags, BlendMode),
      FIELD_ENUM(Stroke, "cap", cap, NoFlags, LineCap),
      FIELD_ENUM(Stroke, "join", join, NoFlags, LineJoin),
      FIELD_FLOAT(Stroke, "miterLimit", miterLimit, Anim),
      FIELD_FLOAT(Stroke, "dashOffset", dashOffset, Anim),
      FIELD_BOOL(Stroke, "dashAdaptive", dashAdaptive, NoFlags),
      FIELD_ENUM(Stroke, "align", align, NoFlags, StrokeAlign),
      FIELD_ENUM(Stroke, "placement", placement, NoFlags, LayerPlacement),
  };
}

static std::vector<ChannelDef> BuildTrimPathFields() {
  return {
      FIELD_FLOAT(TrimPath, "start", start, Anim),
      FIELD_FLOAT(TrimPath, "end", end, Anim),
      FIELD_FLOAT(TrimPath, "offset", offset, Anim),
      FIELD_ENUM(TrimPath, "type", type, NoFlags, TrimType),
  };
}

static std::vector<ChannelDef> BuildRoundCornerFields() {
  return {
      FIELD_FLOAT(RoundCorner, "radius", radius, Anim),
  };
}

static std::vector<ChannelDef> BuildMergePathFields() {
  return {
      FIELD_ENUM(MergePath, "mode", mode, NoFlags, MergePathMode),
  };
}

static std::vector<ChannelDef> BuildTextModifierFields() {
  return {
      FIELD_POINT_X(TextModifier, "anchor.x", anchor, Anim),
      FIELD_POINT_Y(TextModifier, "anchor.y", anchor, Anim),
      FIELD_POINT_X(TextModifier, "position.x", position, Anim),
      FIELD_POINT_Y(TextModifier, "position.y", position, Anim),
      FIELD_FLOAT(TextModifier, "rotation", rotation, Anim),
      FIELD_POINT_X(TextModifier, "scale.x", scale, Anim),
      FIELD_POINT_Y(TextModifier, "scale.y", scale, Anim),
      FIELD_FLOAT(TextModifier, "skew", skew, Anim),
      FIELD_FLOAT(TextModifier, "skewAxis", skewAxis, Anim),
      FIELD_FLOAT(TextModifier, "alpha", alpha, Anim),
      FIELD_OPT_COLOR(TextModifier, "fillColor", fillColor, Anim),
      FIELD_OPT_COLOR(TextModifier, "strokeColor", strokeColor, Anim),
      FIELD_OPT_FLOAT(TextModifier, "strokeWidth", strokeWidth, Anim),
  };
}

static std::vector<ChannelDef> BuildTextPathFields() {
  std::vector<ChannelDef> table = {
      FIELD_POINT_X(TextPath, "baselineOrigin.x", baselineOrigin, Layout),
      FIELD_POINT_Y(TextPath, "baselineOrigin.y", baselineOrigin, Layout),
      FIELD_FLOAT(TextPath, "baselineAngle", baselineAngle, Layout),
      FIELD_FLOAT(TextPath, "firstMargin", firstMargin, Layout),
      FIELD_FLOAT(TextPath, "lastMargin", lastMargin, Layout),
      FIELD_BOOL(TextPath, "perpendicular", perpendicular, Layout),
      FIELD_BOOL(TextPath, "reversed", reversed, NoFlags),
      FIELD_BOOL(TextPath, "forceAlignment", forceAlignment, Layout),
  };
  AppendLayoutNodeFields<TextPath>(table);
  return table;
}

// Shared Group transform/layout fields, also used by TextBox which derives from Group.
template <typename T>
static void AppendGroupCommonFields(std::vector<ChannelDef>& table) {
  std::vector<ChannelDef> shared = {
      FIELD_POINT_X(T, "anchor.x", anchor, Anim),
      FIELD_POINT_Y(T, "anchor.y", anchor, Anim),
      FIELD_POINT_X(T, "position.x", position, AnimLayout),
      FIELD_POINT_Y(T, "position.y", position, AnimLayout),
      FIELD_FLOAT(T, "rotation", rotation, Anim),
      FIELD_POINT_X(T, "scale.x", scale, Anim),
      FIELD_POINT_Y(T, "scale.y", scale, Anim),
      FIELD_FLOAT(T, "skew", skew, Anim),
      FIELD_FLOAT(T, "skewAxis", skewAxis, Anim),
      FIELD_FLOAT(T, "alpha", alpha, Anim),
      FIELD_PADDING_L(T, "padding.left", padding, Layout),
      FIELD_PADDING_T(T, "padding.top", padding, Layout),
      FIELD_PADDING_R(T, "padding.right", padding, Layout),
      FIELD_PADDING_B(T, "padding.bottom", padding, Layout),
  };
  table.insert(table.end(), shared.begin(), shared.end());
  AppendLayoutNodeFields<T>(table);
}

static std::vector<ChannelDef> BuildGroupFields() {
  std::vector<ChannelDef> table = {};
  AppendGroupCommonFields<Group>(table);
  return table;
}

static std::vector<ChannelDef> BuildTextBoxFields() {
  std::vector<ChannelDef> table = {
      FIELD_ENUM(TextBox, "textAlign", textAlign, Layout, TextAlign),
      FIELD_ENUM(TextBox, "paragraphAlign", paragraphAlign, Layout, ParagraphAlign),
      FIELD_ENUM(TextBox, "writingMode", writingMode, Layout, WritingMode),
      FIELD_FLOAT(TextBox, "lineHeight", lineHeight, Layout),
      FIELD_BOOL(TextBox, "wordWrap", wordWrap, Layout),
      FIELD_ENUM(TextBox, "overflow", overflow, Layout, Overflow),
  };
  AppendGroupCommonFields<TextBox>(table);
  return table;
}

static std::vector<ChannelDef> BuildRepeaterFields() {
  return {
      FIELD_FLOAT(Repeater, "copies", copies, Anim),
      FIELD_FLOAT(Repeater, "offset", offset, Anim),
      FIELD_ENUM(Repeater, "order", order, NoFlags, RepeaterOrder),
      FIELD_POINT_X(Repeater, "anchor.x", anchor, Anim),
      FIELD_POINT_Y(Repeater, "anchor.y", anchor, Anim),
      FIELD_POINT_X(Repeater, "position.x", position, Anim),
      FIELD_POINT_Y(Repeater, "position.y", position, Anim),
      FIELD_FLOAT(Repeater, "rotation", rotation, Anim),
      FIELD_POINT_X(Repeater, "scale.x", scale, Anim),
      FIELD_POINT_Y(Repeater, "scale.y", scale, Anim),
      FIELD_FLOAT(Repeater, "startAlpha", startAlpha, Anim),
      FIELD_FLOAT(Repeater, "endAlpha", endAlpha, Anim),
  };
}

static std::vector<ChannelDef> BuildRangeSelectorFields() {
  return {
      FIELD_FLOAT(RangeSelector, "start", start, Anim),
      FIELD_FLOAT(RangeSelector, "end", end, Anim),
      FIELD_FLOAT(RangeSelector, "offset", offset, Anim),
      FIELD_ENUM(RangeSelector, "unit", unit, NoFlags, SelectorUnit),
      FIELD_ENUM(RangeSelector, "shape", shape, NoFlags, SelectorShape),
      FIELD_FLOAT(RangeSelector, "easeIn", easeIn, Anim),
      FIELD_FLOAT(RangeSelector, "easeOut", easeOut, Anim),
      FIELD_ENUM(RangeSelector, "mode", mode, NoFlags, SelectorMode),
      FIELD_FLOAT(RangeSelector, "weight", weight, Anim),
      FIELD_BOOL(RangeSelector, "randomOrder", randomOrder, NoFlags),
      FIELD_INT(RangeSelector, "randomSeed", randomSeed, NoFlags),
  };
}

static std::vector<ChannelDef> BuildSolidColorFields() {
  return {
      FIELD_COLOR(SolidColor, "color", color, Anim),
  };
}

static std::vector<ChannelDef> BuildLinearGradientFields() {
  return {
      FIELD_POINT_X(LinearGradient, "startPoint.x", startPoint, Anim),
      FIELD_POINT_Y(LinearGradient, "startPoint.y", startPoint, Anim),
      FIELD_POINT_X(LinearGradient, "endPoint.x", endPoint, Anim),
      FIELD_POINT_Y(LinearGradient, "endPoint.y", endPoint, Anim),
      FIELD_BOOL(LinearGradient, "fitsToGeometry", fitsToGeometry, NoFlags),
  };
}

static std::vector<ChannelDef> BuildRadialGradientFields() {
  return {
      FIELD_POINT_X(RadialGradient, "center.x", center, Anim),
      FIELD_POINT_Y(RadialGradient, "center.y", center, Anim),
      FIELD_FLOAT(RadialGradient, "radius", radius, Anim),
      FIELD_BOOL(RadialGradient, "fitsToGeometry", fitsToGeometry, NoFlags),
  };
}

static std::vector<ChannelDef> BuildConicGradientFields() {
  return {
      FIELD_POINT_X(ConicGradient, "center.x", center, Anim),
      FIELD_POINT_Y(ConicGradient, "center.y", center, Anim),
      FIELD_FLOAT(ConicGradient, "startAngle", startAngle, Anim),
      FIELD_FLOAT(ConicGradient, "endAngle", endAngle, Anim),
      FIELD_BOOL(ConicGradient, "fitsToGeometry", fitsToGeometry, NoFlags),
  };
}

static std::vector<ChannelDef> BuildDiamondGradientFields() {
  return {
      FIELD_POINT_X(DiamondGradient, "center.x", center, Anim),
      FIELD_POINT_Y(DiamondGradient, "center.y", center, Anim),
      FIELD_FLOAT(DiamondGradient, "radius", radius, Anim),
      FIELD_BOOL(DiamondGradient, "fitsToGeometry", fitsToGeometry, NoFlags),
  };
}

static std::vector<ChannelDef> BuildColorStopFields() {
  return {
      FIELD_FLOAT(ColorStop, "offset", offset, Anim),
      FIELD_COLOR(ColorStop, "color", color, Anim),
  };
}

static std::vector<ChannelDef> BuildImagePatternFields() {
  return {
      FIELD_ENUM(ImagePattern, "tileModeX", tileModeX, NoFlags, TileMode),
      FIELD_ENUM(ImagePattern, "tileModeY", tileModeY, NoFlags, TileMode),
      FIELD_ENUM(ImagePattern, "filterMode", filterMode, NoFlags, FilterMode),
      FIELD_ENUM(ImagePattern, "mipmapMode", mipmapMode, NoFlags, MipmapMode),
      FIELD_ENUM(ImagePattern, "scaleMode", scaleMode, NoFlags, ScaleMode),
  };
}

static std::vector<ChannelDef> BuildDropShadowStyleFields() {
  return {
      FIELD_ENUM(DropShadowStyle, "blendMode", blendMode, NoFlags, BlendMode),
      FIELD_BOOL(DropShadowStyle, "excludeChildEffects", excludeChildEffects, NoFlags),
      FIELD_FLOAT(DropShadowStyle, "offsetX", offsetX, Anim),
      FIELD_FLOAT(DropShadowStyle, "offsetY", offsetY, Anim),
      FIELD_FLOAT(DropShadowStyle, "blurX", blurX, Anim),
      FIELD_FLOAT(DropShadowStyle, "blurY", blurY, Anim),
      FIELD_COLOR(DropShadowStyle, "color", color, Anim),
      FIELD_BOOL(DropShadowStyle, "showBehindLayer", showBehindLayer, Anim),
  };
}

static std::vector<ChannelDef> BuildInnerShadowStyleFields() {
  return {
      FIELD_ENUM(InnerShadowStyle, "blendMode", blendMode, NoFlags, BlendMode),
      FIELD_BOOL(InnerShadowStyle, "excludeChildEffects", excludeChildEffects, NoFlags),
      FIELD_FLOAT(InnerShadowStyle, "offsetX", offsetX, Anim),
      FIELD_FLOAT(InnerShadowStyle, "offsetY", offsetY, Anim),
      FIELD_FLOAT(InnerShadowStyle, "blurX", blurX, Anim),
      FIELD_FLOAT(InnerShadowStyle, "blurY", blurY, Anim),
      FIELD_COLOR(InnerShadowStyle, "color", color, Anim),
  };
}

static std::vector<ChannelDef> BuildBackgroundBlurStyleFields() {
  return {
      FIELD_ENUM(BackgroundBlurStyle, "blendMode", blendMode, NoFlags, BlendMode),
      FIELD_BOOL(BackgroundBlurStyle, "excludeChildEffects", excludeChildEffects, NoFlags),
      FIELD_FLOAT(BackgroundBlurStyle, "blurX", blurX, Anim),
      FIELD_FLOAT(BackgroundBlurStyle, "blurY", blurY, Anim),
      FIELD_ENUM(BackgroundBlurStyle, "tileMode", tileMode, NoFlags, TileMode),
  };
}

static std::vector<ChannelDef> BuildBlurFilterFields() {
  return {
      FIELD_FLOAT(BlurFilter, "blurX", blurX, Anim),
      FIELD_FLOAT(BlurFilter, "blurY", blurY, Anim),
      FIELD_ENUM(BlurFilter, "tileMode", tileMode, NoFlags, TileMode),
  };
}

static std::vector<ChannelDef> BuildDropShadowFilterFields() {
  return {
      FIELD_FLOAT(DropShadowFilter, "offsetX", offsetX, Anim),
      FIELD_FLOAT(DropShadowFilter, "offsetY", offsetY, Anim),
      FIELD_FLOAT(DropShadowFilter, "blurX", blurX, Anim),
      FIELD_FLOAT(DropShadowFilter, "blurY", blurY, Anim),
      FIELD_COLOR(DropShadowFilter, "color", color, Anim),
      FIELD_BOOL(DropShadowFilter, "shadowOnly", shadowOnly, Anim),
  };
}

static std::vector<ChannelDef> BuildInnerShadowFilterFields() {
  return {
      FIELD_FLOAT(InnerShadowFilter, "offsetX", offsetX, Anim),
      FIELD_FLOAT(InnerShadowFilter, "offsetY", offsetY, Anim),
      FIELD_FLOAT(InnerShadowFilter, "blurX", blurX, Anim),
      FIELD_FLOAT(InnerShadowFilter, "blurY", blurY, Anim),
      FIELD_COLOR(InnerShadowFilter, "color", color, Anim),
      FIELD_BOOL(InnerShadowFilter, "shadowOnly", shadowOnly, Anim),
  };
}

static std::vector<ChannelDef> BuildBlendFilterFields() {
  return {
      FIELD_COLOR(BlendFilter, "color", color, Anim),
      FIELD_ENUM(BlendFilter, "blendMode", blendMode, NoFlags, BlendMode),
  };
}

const std::vector<ChannelDef>& ChannelsFor(NodeType type) {
  static const std::vector<ChannelDef> empty = {};
  // Each table is built once on first use. Static locals keep the member-pointer-generated access
  // functions and channel rows alive for the process lifetime.
  switch (type) {
    case NodeType::Layer: {
      static const std::vector<ChannelDef> table = BuildLayerFields();
      return table;
    }
    case NodeType::Rectangle: {
      static const std::vector<ChannelDef> table = BuildRectangleFields();
      return table;
    }
    case NodeType::Ellipse: {
      static const std::vector<ChannelDef> table = BuildEllipseFields();
      return table;
    }
    case NodeType::Polystar: {
      static const std::vector<ChannelDef> table = BuildPolystarFields();
      return table;
    }
    case NodeType::Path: {
      static const std::vector<ChannelDef> table = BuildPathFields();
      return table;
    }
    case NodeType::Text: {
      static const std::vector<ChannelDef> table = BuildTextFields();
      return table;
    }
    case NodeType::Fill: {
      static const std::vector<ChannelDef> table = BuildFillFields();
      return table;
    }
    case NodeType::Stroke: {
      static const std::vector<ChannelDef> table = BuildStrokeFields();
      return table;
    }
    case NodeType::TrimPath: {
      static const std::vector<ChannelDef> table = BuildTrimPathFields();
      return table;
    }
    case NodeType::RoundCorner: {
      static const std::vector<ChannelDef> table = BuildRoundCornerFields();
      return table;
    }
    case NodeType::MergePath: {
      static const std::vector<ChannelDef> table = BuildMergePathFields();
      return table;
    }
    case NodeType::TextModifier: {
      static const std::vector<ChannelDef> table = BuildTextModifierFields();
      return table;
    }
    case NodeType::TextPath: {
      static const std::vector<ChannelDef> table = BuildTextPathFields();
      return table;
    }
    case NodeType::TextBox: {
      static const std::vector<ChannelDef> table = BuildTextBoxFields();
      return table;
    }
    case NodeType::Group: {
      static const std::vector<ChannelDef> table = BuildGroupFields();
      return table;
    }
    case NodeType::Repeater: {
      static const std::vector<ChannelDef> table = BuildRepeaterFields();
      return table;
    }
    case NodeType::RangeSelector: {
      static const std::vector<ChannelDef> table = BuildRangeSelectorFields();
      return table;
    }
    case NodeType::SolidColor: {
      static const std::vector<ChannelDef> table = BuildSolidColorFields();
      return table;
    }
    case NodeType::LinearGradient: {
      static const std::vector<ChannelDef> table = BuildLinearGradientFields();
      return table;
    }
    case NodeType::RadialGradient: {
      static const std::vector<ChannelDef> table = BuildRadialGradientFields();
      return table;
    }
    case NodeType::ConicGradient: {
      static const std::vector<ChannelDef> table = BuildConicGradientFields();
      return table;
    }
    case NodeType::DiamondGradient: {
      static const std::vector<ChannelDef> table = BuildDiamondGradientFields();
      return table;
    }
    case NodeType::ColorStop: {
      static const std::vector<ChannelDef> table = BuildColorStopFields();
      return table;
    }
    case NodeType::ImagePattern: {
      static const std::vector<ChannelDef> table = BuildImagePatternFields();
      return table;
    }
    case NodeType::DropShadowStyle: {
      static const std::vector<ChannelDef> table = BuildDropShadowStyleFields();
      return table;
    }
    case NodeType::InnerShadowStyle: {
      static const std::vector<ChannelDef> table = BuildInnerShadowStyleFields();
      return table;
    }
    case NodeType::BackgroundBlurStyle: {
      static const std::vector<ChannelDef> table = BuildBackgroundBlurStyleFields();
      return table;
    }
    case NodeType::BlurFilter: {
      static const std::vector<ChannelDef> table = BuildBlurFilterFields();
      return table;
    }
    case NodeType::DropShadowFilter: {
      static const std::vector<ChannelDef> table = BuildDropShadowFilterFields();
      return table;
    }
    case NodeType::InnerShadowFilter: {
      static const std::vector<ChannelDef> table = BuildInnerShadowFilterFields();
      return table;
    }
    case NodeType::BlendFilter: {
      static const std::vector<ChannelDef> table = BuildBlendFilterFields();
      return table;
    }
    default:
      return empty;
  }
}

// Builds the per-type channel lookup map from the vector, keyed by string_view referencing the
// ChannelDef's channel const char*. The literal strings have static storage duration so the
// string_view keys remain valid for the program lifetime.
static std::unordered_map<std::string_view, const ChannelDef*> BuildChannelIndex(
    const std::vector<ChannelDef>& table) {
  std::unordered_map<std::string_view, const ChannelDef*> index = {};
  index.reserve(table.size());
  for (const auto& field : table) {
    index.emplace(std::string_view(field.channel), &field);
  }
  return index;
}

// Returns the per-type O(1) lookup index over ChannelsFor(type). The index is a process-wide cache
// keyed by &table: each type's vector address maps to its built-once
// unordered_map<string_view, const ChannelDef*>. The cache is not synchronized: PAGX reflection
// APIs are document-level edit operations and are expected to run on a single thread (matching the
// rest of the PAGXDocument editing surface), so the first-call insert is not racing with reads in
// any supported scenario. Underlying ChannelDef::channel pointers are static const char* literals,
// so string_view keys outlive any caller; the unique_ptr keeps each inner map at a stable address
// across cache rehashes.
static const std::unordered_map<std::string_view, const ChannelDef*>& ChannelIndexFor(
    NodeType type) {
  using ChannelIndex = std::unordered_map<std::string_view, const ChannelDef*>;
  const auto& table = ChannelsFor(type);
  static std::unordered_map<const std::vector<ChannelDef>*, std::unique_ptr<ChannelIndex>> cache =
      {};
  auto it = cache.find(&table);
  if (it == cache.end()) {
    auto index = std::unique_ptr<ChannelIndex>(new ChannelIndex(BuildChannelIndex(table)));
    it = cache.emplace(&table, std::move(index)).first;
  }
  return *it->second;
}

static const ChannelDef* FindChannel(NodeType type, const std::string& channel) {
  const auto& index = ChannelIndexFor(type);
  auto found = index.find(std::string_view(channel));
  return found != index.end() ? found->second : nullptr;
}

bool GetNodeChannel(const Node* node, const std::string& channel, KeyValue* out) {
  if (node == nullptr || out == nullptr) {
    return false;
  }
  const auto* field = FindChannel(node->nodeType(), channel);
  if (field == nullptr) {
    LOGE("GetNodeChannel: unhandled channel '%s' for node type %d.", channel.c_str(),
         static_cast<int>(node->nodeType()));
    return false;
  }
  // The read path never mutates the node; const_cast is safe because access only writes when setIn
  // is non-null, which it is not here.
  if (!field->access(const_cast<Node*>(node), out, nullptr)) {
    LOGE("GetNodeChannel: failed to read channel '%s' for node type %d.", channel.c_str(),
         static_cast<int>(node->nodeType()));
    return false;
  }
  return true;
}

bool SetNodeChannel(Node* node, const std::string& channel, const KeyValue& value) {
  if (node == nullptr) {
    return false;
  }
  const auto* field = FindChannel(node->nodeType(), channel);
  if (field == nullptr || !field->access(node, nullptr, &value)) {
    LOGE("SetNodeChannel: unhandled channel '%s' or value type mismatch for node type %d.",
         channel.c_str(), static_cast<int>(node->nodeType()));
    return false;
  }
  return true;
}

bool ResetNodeChannel(Node* node, const std::string& channel) {
  if (node == nullptr) {
    return false;
  }
  const auto* field = FindChannel(node->nodeType(), channel);
  if (field == nullptr || !field->access(node, nullptr, nullptr)) {
    LOGE("ResetNodeChannel: unhandled channel '%s' for node type %d.", channel.c_str(),
         static_cast<int>(node->nodeType()));
    return false;
  }
  return true;
}

bool IsAnimatableChannel(NodeType type, const std::string& channel) {
  const auto* field = FindChannel(type, channel);
  return field != nullptr && HasFlag(field->flags, ChannelFlags::Animatable);
}

bool RequiresLayout(NodeType type, const std::string& channel) {
  const auto* field = FindChannel(type, channel);
  return field != nullptr && HasFlag(field->flags, ChannelFlags::RequiresLayout);
}

bool ChannelExists(NodeType type, const std::string& channel) {
  return FindChannel(type, channel) != nullptr;
}

std::vector<std::string> ListChannels(NodeType type) {
  const auto& table = ChannelsFor(type);
  std::vector<std::string> names = {};
  names.reserve(table.size());
  for (const auto& field : table) {
    names.emplace_back(field.channel);
  }
  return names;
}

}  // namespace pagx
