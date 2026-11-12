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

#include "pagx/svg/SVGCoverageLedger.h"
#include <unordered_map>

namespace pagx {

namespace {

constexpr auto S = SVGChannelStatus::Supported;
constexpr auto P = SVGChannelStatus::Planned;
constexpr auto X = SVGChannelStatus::NotExpressible;

std::vector<SVGChannelEntry> BuildLedger() {
  std::vector<SVGChannelEntry> entries = {};

  // Layer
  entries.push_back({NodeType::Layer, "visible", S, "animate visibility discrete"});
  entries.push_back({NodeType::Layer, "alpha", S, "animate opacity on the wrapping g"});
  entries.push_back({NodeType::Layer, "blendMode", X, "mix-blend-mode is a CSS property"});
  entries.push_back({NodeType::Layer, "x", P, "needs the full transform model"});
  entries.push_back({NodeType::Layer, "y", P, "needs the full transform model"});
  entries.push_back({NodeType::Layer, "z", P, "3D, bake only"});
  entries.push_back({NodeType::Layer, "rotation.x", P, "3D, bake only"});
  entries.push_back({NodeType::Layer, "rotation.y", P, "3D, bake only"});
  entries.push_back({NodeType::Layer, "rotation.z", P, "needs the full transform model"});
  entries.push_back({NodeType::Layer, "scale.x", P, "needs the full transform model"});
  entries.push_back({NodeType::Layer, "scale.y", P, "needs the full transform model"});
  entries.push_back({NodeType::Layer, "scale.z", P, "3D, bake only"});
  entries.push_back({NodeType::Layer, "skew.x", P, "needs the full transform model"});
  entries.push_back({NodeType::Layer, "skew.y", P, "needs the full transform model"});
  entries.push_back({NodeType::Layer, "perspective", P, "3D, bake only"});
  entries.push_back({NodeType::Layer, "matrix", S, "decomposed into four animateTransform tracks"});
  entries.push_back({NodeType::Layer, "matrix3D", P, "3D, bake only"});
  entries.push_back({NodeType::Layer, "anchor.x", P, "needs the full transform model"});
  entries.push_back({NodeType::Layer, "anchor.y", P, "needs the full transform model"});
  entries.push_back({NodeType::Layer, "anchor.z", P, "3D, bake only"});

  // Group / TextBox share the same animation contract.
  for (auto type : {NodeType::Group, NodeType::TextBox}) {
    entries.push_back({type, "anchor.x", S, "pushed down to descendant elements"});
    entries.push_back({type, "anchor.y", S, "pushed down to descendant elements"});
    entries.push_back({type, "position.x", S, "pushed down to descendant elements"});
    entries.push_back({type, "position.y", S, "pushed down to descendant elements"});
    entries.push_back({type, "rotation", S, "pushed down to descendant elements"});
    entries.push_back({type, "scale.x", S, "pushed down to descendant elements"});
    entries.push_back({type, "scale.y", S, "pushed down to descendant elements"});
    entries.push_back({type, "skew", S, "pushed down to descendant elements"});
    entries.push_back({type, "skewAxis", S, "pushed down to descendant elements"});
    entries.push_back({type, "alpha", S, "pushed down as fill/stroke-opacity"});
  }

  // Rectangle
  entries.push_back({NodeType::Rectangle, "position.x", S, "animate x on the rect"});
  entries.push_back({NodeType::Rectangle, "position.y", S, "animate y on the rect"});
  entries.push_back({NodeType::Rectangle, "size.width", S, "animate width on the rect"});
  entries.push_back({NodeType::Rectangle, "size.height", S, "animate height on the rect"});
  entries.push_back({NodeType::Rectangle, "roundness", S, "animate rx and ry"});

  // Ellipse
  entries.push_back({NodeType::Ellipse, "position.x", S, "animate cx"});
  entries.push_back({NodeType::Ellipse, "position.y", S, "animate cy"});
  entries.push_back({NodeType::Ellipse, "size.width", S, "animate r or rx"});
  entries.push_back({NodeType::Ellipse, "size.height", S, "animate r or ry"});

  // Polystar
  entries.push_back({NodeType::Polystar, "position.x", S, "d track with recomputed placement"});
  entries.push_back({NodeType::Polystar, "position.y", S, "d track with recomputed placement"});
  entries.push_back({NodeType::Polystar, "pointCount", P, "topology change, needs a d track"});
  entries.push_back({NodeType::Polystar, "outerRadius", S, "d track, fixed topology"});
  entries.push_back({NodeType::Polystar, "innerRadius", S, "d track, fixed topology"});
  entries.push_back({NodeType::Polystar, "rotation", S, "d track, fixed topology"});
  entries.push_back(
      {NodeType::Polystar, "outerRoundness", S, "d track on the baked path, fixed topology"});
  entries.push_back(
      {NodeType::Polystar, "innerRoundness", S, "d track on the baked path, fixed topology"});

  // Text
  entries.push_back({NodeType::Text, "text", P, "layout reshape, bake only"});
  entries.push_back({NodeType::Text, "x", S, "wrapping g translate, no reshape"});
  entries.push_back({NodeType::Text, "y", S, "wrapping g translate, no reshape"});
  entries.push_back({NodeType::Text, "fontFamily", P, "layout reshape, bake only"});
  entries.push_back({NodeType::Text, "fontStyle", P, "layout reshape, bake only"});
  entries.push_back({NodeType::Text, "fontSize", P, "layout reshape, bake only"});
  entries.push_back({NodeType::Text, "letterSpacing", P, "layout reshape, bake only"});
  entries.push_back({NodeType::Text, "fauxBold", P, "layout reshape, bake only"});
  entries.push_back({NodeType::Text, "fauxItalic", P, "layout reshape, bake only"});

  // Fill / Stroke
  entries.push_back({NodeType::Fill, "color", S, "animate fill and fill-opacity (solid only)"});
  entries.push_back({NodeType::Fill, "alpha", S, "animate fill-opacity"});
  entries.push_back({NodeType::Stroke, "color", S, "animate stroke and stroke-opacity"});
  entries.push_back({NodeType::Stroke, "width", S, "animate stroke-width"});
  entries.push_back({NodeType::Stroke, "alpha", S, "animate stroke-opacity"});
  entries.push_back({NodeType::Stroke, "miterLimit", S, "animate stroke-miterlimit"});
  entries.push_back({NodeType::Stroke, "dashOffset", S, "animate stroke-dashoffset"});

  // Path modifiers
  entries.push_back({NodeType::TrimPath, "start", P, "needs a d track"});
  entries.push_back({NodeType::TrimPath, "end", P, "needs a d track"});
  entries.push_back({NodeType::TrimPath, "offset", P, "needs a d track"});
  entries.push_back({NodeType::RoundCorner, "radius", P, "needs a d track"});
  entries.push_back({NodeType::Repeater, "copies", P, "structural, bake only"});
  entries.push_back({NodeType::Repeater, "offset", P, "per-copy transform, not implemented"});
  entries.push_back({NodeType::Repeater, "anchor.x", P, "per-copy transform, not implemented"});
  entries.push_back({NodeType::Repeater, "anchor.y", P, "per-copy transform, not implemented"});
  entries.push_back({NodeType::Repeater, "position.x", P, "per-copy transform, not implemented"});
  entries.push_back({NodeType::Repeater, "position.y", P, "per-copy transform, not implemented"});
  entries.push_back({NodeType::Repeater, "rotation", P, "per-copy transform, not implemented"});
  entries.push_back({NodeType::Repeater, "scale.x", P, "per-copy transform, not implemented"});
  entries.push_back({NodeType::Repeater, "scale.y", P, "per-copy transform, not implemented"});
  entries.push_back({NodeType::Repeater, "startAlpha", P, "per-copy alpha, not implemented"});
  entries.push_back({NodeType::Repeater, "endAlpha", P, "per-copy alpha, not implemented"});

  // Text modifiers
  entries.push_back({NodeType::TextModifier, "anchor.x", P, "per-glyph transform, bake only"});
  entries.push_back({NodeType::TextModifier, "anchor.y", P, "per-glyph transform, bake only"});
  entries.push_back({NodeType::TextModifier, "position.x", P, "per-glyph transform, bake only"});
  entries.push_back({NodeType::TextModifier, "position.y", P, "per-glyph transform, bake only"});
  entries.push_back({NodeType::TextModifier, "rotation", P, "per-glyph transform, bake only"});
  entries.push_back({NodeType::TextModifier, "scale.x", P, "per-glyph transform, bake only"});
  entries.push_back({NodeType::TextModifier, "scale.y", P, "per-glyph transform, bake only"});
  entries.push_back({NodeType::TextModifier, "skew", P, "per-glyph transform, bake only"});
  entries.push_back({NodeType::TextModifier, "skewAxis", P, "per-glyph transform, bake only"});
  entries.push_back({NodeType::TextModifier, "alpha", P, "per-glyph style, bake only"});
  entries.push_back({NodeType::TextModifier, "fillColor", P, "per-glyph style, bake only"});
  entries.push_back({NodeType::TextModifier, "strokeColor", P, "per-glyph style, bake only"});
  entries.push_back({NodeType::TextModifier, "strokeWidth", P, "per-glyph style, bake only"});
  entries.push_back({NodeType::RangeSelector, "start", P, "glyph factor, not implemented"});
  entries.push_back({NodeType::RangeSelector, "end", P, "glyph factor, not implemented"});
  entries.push_back({NodeType::RangeSelector, "offset", P, "glyph factor, not implemented"});
  entries.push_back({NodeType::RangeSelector, "easeIn", P, "glyph factor, not implemented"});
  entries.push_back({NodeType::RangeSelector, "easeOut", P, "glyph factor, not implemented"});
  entries.push_back({NodeType::RangeSelector, "weight", P, "glyph factor, not implemented"});

  // Color sources
  entries.push_back(
      {NodeType::SolidColor, "color", S, "shared source drives every Fill/Stroke consumer"});
  entries.push_back({NodeType::LinearGradient, "startPoint.x", S, "animate x1"});
  entries.push_back({NodeType::LinearGradient, "startPoint.y", S, "animate y1"});
  entries.push_back({NodeType::LinearGradient, "endPoint.x", S, "animate x2"});
  entries.push_back({NodeType::LinearGradient, "endPoint.y", S, "animate y2"});
  entries.push_back({NodeType::RadialGradient, "center.x", S, "animate cx"});
  entries.push_back({NodeType::RadialGradient, "center.y", S, "animate cy"});
  entries.push_back({NodeType::RadialGradient, "radius", S, "animate r, negative samples clamped"});
  entries.push_back(
      {NodeType::ConicGradient, "center.x", X, "conic degrades to a radial approximation"});
  entries.push_back(
      {NodeType::ConicGradient, "center.y", X, "conic degrades to a radial approximation"});
  entries.push_back(
      {NodeType::ConicGradient, "startAngle", X, "conic degrades to a radial approximation"});
  entries.push_back(
      {NodeType::ConicGradient, "endAngle", X, "conic degrades to a radial approximation"});
  entries.push_back(
      {NodeType::DiamondGradient, "center.x", X, "diamond degrades to a radial approximation"});
  entries.push_back(
      {NodeType::DiamondGradient, "center.y", X, "diamond degrades to a radial approximation"});
  entries.push_back(
      {NodeType::DiamondGradient, "radius", X, "diamond degrades to a radial approximation"});
  entries.push_back(
      {NodeType::ColorStop, "offset", S, "animate stop offset in linear/radial defs"});
  entries.push_back({NodeType::ColorStop, "color", S,
                     "stop-color/stop-opacity in linear/radial; conic/diamond "
                     "consumers are rejected"});

  // Layer styles
  entries.push_back({NodeType::DropShadowStyle, "offsetX", S, "animate feOffset dx"});
  entries.push_back({NodeType::DropShadowStyle, "offsetY", S, "animate feOffset dy"});
  entries.push_back({NodeType::DropShadowStyle, "blurX", S, "animate feGaussianBlur stdDeviation"});
  entries.push_back({NodeType::DropShadowStyle, "blurY", S, "animate feGaussianBlur stdDeviation"});
  entries.push_back({NodeType::DropShadowStyle, "spread", P, "spread is not exported at all"});
  entries.push_back({NodeType::DropShadowStyle, "color", P, "baked into feColorMatrix constants"});
  entries.push_back({NodeType::DropShadowStyle, "showBehindLayer", P, "structural, feMerge order"});
  entries.push_back({NodeType::InnerShadowStyle, "offsetX", S, "animate feOffset dx"});
  entries.push_back({NodeType::InnerShadowStyle, "offsetY", S, "animate feOffset dy"});
  entries.push_back(
      {NodeType::InnerShadowStyle, "blurX", S, "animate feGaussianBlur stdDeviation"});
  entries.push_back(
      {NodeType::InnerShadowStyle, "blurY", S, "animate feGaussianBlur stdDeviation"});
  entries.push_back({NodeType::InnerShadowStyle, "spread", P, "spread is not exported at all"});
  entries.push_back({NodeType::InnerShadowStyle, "color", P, "baked into feColorMatrix constants"});
  entries.push_back({NodeType::BackgroundBlurStyle, "blurX", P, "no backdrop blur in SVG"});
  entries.push_back({NodeType::BackgroundBlurStyle, "blurY", P, "no backdrop blur in SVG"});
  entries.push_back({NodeType::GlassStyle, "refraction", P, "no glass primitive in SVG"});
  entries.push_back({NodeType::GlassStyle, "depth", P, "no glass primitive in SVG"});
  entries.push_back({NodeType::GlassStyle, "frost", P, "no glass primitive in SVG"});
  entries.push_back({NodeType::GlassStyle, "dispersion", P, "no glass primitive in SVG"});
  entries.push_back({NodeType::GlassStyle, "splay", P, "no glass primitive in SVG"});
  entries.push_back({NodeType::GlassStyle, "lightAngle", P, "no glass primitive in SVG"});
  entries.push_back({NodeType::GlassStyle, "lightIntensity", P, "no glass primitive in SVG"});
  entries.push_back({NodeType::NoiseStyle, "size", P, "non-linear baseFrequency mapping"});
  entries.push_back({NodeType::NoiseStyle, "density", P, "not implemented"});
  entries.push_back({NodeType::NoiseStyle, "seed", P, "animate feTurbulence seed"});
  entries.push_back({NodeType::NoiseStyle, "color", P, "feFlood per mode"});
  entries.push_back({NodeType::NoiseStyle, "firstColor", P, "feFlood per mode"});
  entries.push_back({NodeType::NoiseStyle, "secondColor", P, "feFlood per mode"});
  entries.push_back({NodeType::NoiseStyle, "opacity", P, "feComponentTransfer per mode"});

  // Layer filters
  entries.push_back({NodeType::BlurFilter, "blurX", S, "animate feGaussianBlur stdDeviation"});
  entries.push_back({NodeType::BlurFilter, "blurY", S, "animate feGaussianBlur stdDeviation"});
  entries.push_back({NodeType::DropShadowFilter, "offsetX", S, "animate feOffset dx"});
  entries.push_back({NodeType::DropShadowFilter, "offsetY", S, "animate feOffset dy"});
  entries.push_back(
      {NodeType::DropShadowFilter, "blurX", S, "animate feGaussianBlur stdDeviation"});
  entries.push_back(
      {NodeType::DropShadowFilter, "blurY", S, "animate feGaussianBlur stdDeviation"});
  entries.push_back({NodeType::DropShadowFilter, "color", P, "baked into feColorMatrix constants"});
  entries.push_back({NodeType::DropShadowFilter, "shadowOnly", P, "structural, feMerge order"});
  entries.push_back({NodeType::InnerShadowFilter, "offsetX", S, "animate feOffset dx"});
  entries.push_back({NodeType::InnerShadowFilter, "offsetY", S, "animate feOffset dy"});
  entries.push_back(
      {NodeType::InnerShadowFilter, "blurX", S, "animate feGaussianBlur stdDeviation"});
  entries.push_back(
      {NodeType::InnerShadowFilter, "blurY", S, "animate feGaussianBlur stdDeviation"});
  entries.push_back(
      {NodeType::InnerShadowFilter, "color", P, "baked into feColorMatrix constants"});
  entries.push_back({NodeType::InnerShadowFilter, "shadowOnly", P, "structural, feMerge order"});
  entries.push_back({NodeType::BlendFilter, "color", S, "animate feFlood flood-color"});
  entries.push_back({NodeType::NoiseFilter, "size", P, "non-linear baseFrequency mapping"});
  entries.push_back({NodeType::NoiseFilter, "density", P, "not implemented"});
  entries.push_back({NodeType::NoiseFilter, "seed", P, "animate feTurbulence seed"});
  entries.push_back({NodeType::NoiseFilter, "color", P, "feFlood per mode"});
  entries.push_back({NodeType::NoiseFilter, "firstColor", P, "feFlood per mode"});
  entries.push_back({NodeType::NoiseFilter, "secondColor", P, "feFlood per mode"});
  entries.push_back({NodeType::NoiseFilter, "opacity", P, "feComponentTransfer per mode"});

  return entries;
}

std::string LedgerKey(NodeType type, const std::string& channel) {
  return std::to_string(static_cast<int>(type)) + "/" + channel;
}

struct LedgerIndex {
  std::unordered_map<std::string, const SVGChannelEntry*> map = {};

  LedgerIndex() {
    for (const auto& entry : SVGChannelLedger()) {
      map[LedgerKey(entry.type, entry.channel)] = &entry;
    }
  }
};

const std::unordered_map<std::string, const SVGChannelEntry*>& GetLedgerIndex() {
  static const LedgerIndex index = {};
  return index.map;
}

}  // namespace

const std::vector<SVGChannelEntry>& SVGChannelLedger() {
  static const std::vector<SVGChannelEntry> entries = BuildLedger();
  return entries;
}

const SVGChannelEntry* FindSVGChannelEntry(NodeType type, const std::string& channel) {
  const auto& index = GetLedgerIndex();
  auto iter = index.find(LedgerKey(type, channel));
  return iter == index.end() ? nullptr : iter->second;
}

size_t SVGClassifiedNodeTypeCount() {
  std::unordered_map<int, bool> types = {};
  for (const auto& entry : SVGChannelLedger()) {
    types[static_cast<int>(entry.type)] = true;
  }
  return types.size();
}

}  // namespace pagx
