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

#include "pagx/html/HTMLSubsetPropertyTable.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include "pagx/html/HTMLParserContext.h"

namespace pagx::subset_props {

using pagx::detail::SplitTopLevelWhitespace;
using pagx::detail::ToLower;
using pagx::detail::Trim;

namespace {

constexpr float kRemBasePx = 16.0f;

bool IsFiniteNonZero(float v) {
  return std::isfinite(v) && v != 0.0f;
}

// Parses a CSS length component into a `(number, unit)` pair. Returns false when the value is
// not a number followed by an optional unit.
bool ParseLengthComponent(const std::string& trimmed, float& outNumber, std::string& outUnit) {
  if (trimmed.empty()) return false;
  char* end = nullptr;
  float v = std::strtof(trimmed.c_str(), &end);
  if (end == trimmed.c_str()) return false;
  outNumber = v;
  outUnit = Trim(end);
  return true;
}

std::string FormatNumber(float v) {
  // Use a stringstream with enough precision but no trailing zeros / scientific notation for
  // small integers. CSS doesn't care about scale here; deterministic output matters.
  if (std::isfinite(v)) {
    float rounded = std::round(v * 1000.0f) / 1000.0f;
    if (rounded == std::floor(rounded) && std::fabs(rounded) < 1e9f) {
      std::ostringstream oss;
      oss << static_cast<long long>(rounded);
      return oss.str();
    }
  }
  std::ostringstream oss;
  oss.precision(6);
  oss << v;
  return oss.str();
}

// Re-stringifies a length as `<n>px` / `<n>%`.
std::string EmitPx(float px) {
  return FormatNumber(px) + "px";
}
std::string EmitPercent(float pct) {
  return FormatNumber(pct) + "%";
}

std::string DropProperty(const std::string& propertyName, const std::string& value,
                         const std::string& reason, HTMLTransformContext& diagnostics) {
  diagnostics.warn("subset:unsupported-property",
                   "html: '" + propertyName + ": " + value + "' " + reason);
  return std::string();
}

// ----------------------- Per-property transforms ---------------------------------------------

std::string TransformDisplay(const std::string& value, const PropertyContext& ctx,
                             HTMLTransformContext& diags) {
  std::string lc = ToLower(Trim(value));
  if (lc == "flex" || lc == "block") return lc;
  if (lc == "inline" || lc == "inline-block") {
    diags.warn("subset:unsupported-property",
               "html: 'display: " + lc + "' on '" + ctx.ownerTag + "' is downgraded to block");
    return "block";
  }
  return DropProperty("display", value, "is not supported", diags);
}

std::string TransformFlexDirection(const std::string& value, const PropertyContext&,
                                   HTMLTransformContext& diags) {
  std::string lc = ToLower(Trim(value));
  if (lc == "row" || lc == "column") return lc;
  if (lc == "row-reverse" || lc == "column-reverse") {
    diags.warn("subset:unsupported-property", "html: 'flex-direction: " + lc +
                                                  "' is not supported; falling back to '" +
                                                  (lc == "row-reverse" ? "row" : "column") + "'");
    return lc == "row-reverse" ? "row" : "column";
  }
  return DropProperty("flex-direction", value, "is not a recognised keyword", diags);
}

std::string TransformAlignItems(const std::string& value, const PropertyContext&,
                                HTMLTransformContext& diags) {
  std::string lc = ToLower(Trim(value));
  if (lc == "stretch" || lc == "center" || lc == "flex-start" || lc == "flex-end" ||
      lc == "start" || lc == "end" || lc == "baseline") {
    return lc;
  }
  return DropProperty("align-items", value, "is not a recognised keyword", diags);
}

std::string TransformJustifyContent(const std::string& value, const PropertyContext&,
                                    HTMLTransformContext& diags) {
  std::string lc = ToLower(Trim(value));
  if (lc == "flex-start" || lc == "flex-end" || lc == "start" || lc == "end" || lc == "center" ||
      lc == "space-between" || lc == "space-around" || lc == "space-evenly") {
    return lc;
  }
  return DropProperty("justify-content", value, "is not a recognised keyword", diags);
}

std::string TransformFlexShrink(const std::string& value, const PropertyContext&,
                                HTMLTransformContext& diags) {
  // `flex-shrink: 0` is emitted on every flex item by the html-snapshot tool so the
  // subset HTML renders identically in a real browser (the live phone-shaped pages
  // it targets routinely produce flex columns whose natural content height exceeds
  // their declared size; the default `flex-shrink: 1` would crush each item and
  // strand its absolutely-positioned descendants). PAGX's flex engine never shrinks
  // items either, so `flex-shrink: 0` is a verbatim no-op for our renderer — drop
  // it silently to keep import logs readable. Any other value still hits the warning
  // path: it would imply a layout that PAGX cannot honour and the author should know.
  std::string lc = ToLower(Trim(value));
  if (lc == "0" || lc == "0.0") return std::string();
  return DropProperty("flex-shrink", value, "use 'flex: <N>' shorthand instead", diags);
}

std::string TransformFlex(const std::string& value, const PropertyContext&,
                          HTMLTransformContext& diags) {
  std::string trimmed = Trim(value);
  if (trimmed.empty()) return std::string();
  // Subset: only `flex: <grow>` is accepted. Collapse the full shorthand by keeping the grow
  // component when the shrink/basis components do not change behaviour (`grow grow 0%` is the
  // common Tailwind form).
  auto tokens = SplitTopLevelWhitespace(trimmed);
  float grow = 0.0f;
  std::string unit;
  if (tokens.empty()) return std::string();
  if (!ParseLengthComponent(tokens[0], grow, unit)) {
    // `flex: none | auto | initial` etc.
    std::string lc = ToLower(trimmed);
    if (lc == "none") return "0";
    if (lc == "auto" || lc == "initial") return "1";
    return DropProperty("flex", value, "is not a recognised shorthand", diags);
  }
  if (!unit.empty()) {
    diags.warn("subset:unsupported-property",
               "html: 'flex' grow component must be unitless; got '" + tokens[0] +
                   "'. Using numeric portion.");
  }
  if (tokens.size() > 1) {
    diags.warn("subset:flex-shorthand-collapsed",
               "html: flex shorthand 'flex: " + trimmed +
                   "' collapsed to 'flex: " + FormatNumber(grow) + "' (subset accepts grow only)");
  }
  return FormatNumber(grow);
}

std::string TransformLength(const std::string& value, const PropertyContext& ctx,
                            HTMLTransformContext& diags) {
  return ResolveLength(value, ctx, diags);
}

std::string TransformLengthShorthand(const std::string& value, const PropertyContext& ctx,
                                     HTMLTransformContext& diags) {
  return ResolveLengthShorthand(value, ctx, diags);
}

std::string TransformPosition(const std::string& value, const PropertyContext& ctx,
                              HTMLTransformContext& diags) {
  std::string lc = ToLower(Trim(value));
  if (lc == "absolute") return "absolute";
  if (lc == "static") return std::string();  // default; drop silently
  // `position: relative` in CSS keeps the element in normal flow and only
  // establishes a containing block for absolutely-positioned descendants. PAGX
  // has no containing-block concept — child layers always anchor to their direct
  // parent — so the declaration has no PAGX-side effect. Drop it silently and
  // leave the element in flow, which matches both the CSS visual (when no
  // offsets are paired with it) and the PAGX semantics for the surrounding
  // layout. `tools/html-snapshot/snapshot.js` emits this on flex items
  // specifically to give a real browser a containing block for the abs
  // descendants in the subtree; downgrading to `absolute` instead would yank
  // those flex items out of the parent's flex flow on import.
  if (lc == "relative") return std::string();
  if (lc == "fixed" || lc == "sticky") {
    diags.warn("subset:unsupported-property",
               "html: 'position: " + lc + "' on '" + ctx.ownerTag + "' downgraded to absolute");
    return "absolute";
  }
  return DropProperty("position", value, "is not a recognised keyword", diags);
}

std::string TransformBackgroundImage(const std::string& value, const PropertyContext&,
                                     HTMLTransformContext& diags) {
  std::string lc = ToLower(value);
  if (lc.find("linear-gradient") != std::string::npos ||
      lc.find("radial-gradient") != std::string::npos ||
      lc.find("conic-gradient") != std::string::npos) {
    return Trim(value);
  }
  if (lc.find("url(") != std::string::npos) {
    return DropProperty("background-image", value, "url() backgrounds require <img/> instead",
                        diags);
  }
  if (Trim(lc) == "none") return std::string();
  return DropProperty("background-image", value, "is not a supported value", diags);
}

std::string TransformBorder(const std::string& value, const PropertyContext&,
                            HTMLTransformContext& diags) {
  std::string trimmed = Trim(value);
  if (trimmed.empty()) return std::string();
  std::string lc = ToLower(trimmed);
  if (lc == "none" || lc == "0" || lc == "0px") return std::string();
  // Accept the canonical `Wpx solid <color>` form (with arbitrary token order). The importer's
  // own parser tolerates ordering, so we just sanity-check and pass through.
  auto tokens = SplitTopLevelWhitespace(trimmed);
  bool sawWidth = false;
  bool sawStyle = false;
  for (auto& t : tokens) {
    std::string tl = ToLower(t);
    if (tl == "solid") {
      sawStyle = true;
      continue;
    }
    if (tl == "dashed" || tl == "dotted" || tl == "double" || tl == "groove" || tl == "ridge" ||
        tl == "inset" || tl == "outset" || tl == "none" || tl == "hidden") {
      diags.warn("subset:unsupported-property",
                 "html: 'border' style '" + tl + "' downgraded to 'solid'");
      sawStyle = true;
      continue;
    }
    float n = 0.0f;
    std::string unit;
    if (ParseLengthComponent(t, n, unit)) {
      sawWidth = true;
      continue;
    }
    // Otherwise assume color: pass through.
  }
  (void)sawWidth;
  (void)sawStyle;
  return trimmed;
}

std::string TransformBorderRadius(const std::string& value, const PropertyContext& ctx,
                                  HTMLTransformContext& diags) {
  // `border-radius` accepts 1-4 lengths or percentages; reuse the length shorthand resolver.
  return ResolveLengthShorthand(value, ctx, diags);
}

std::string TransformOpacity(const std::string& value, const PropertyContext&,
                             HTMLTransformContext& diags) {
  std::string trimmed = Trim(value);
  if (trimmed.empty()) return std::string();
  char* end = nullptr;
  float v = std::strtof(trimmed.c_str(), &end);
  if (end == trimmed.c_str()) {
    return DropProperty("opacity", value, "is not a number", diags);
  }
  std::string unit = Trim(end);
  if (unit == "%") v /= 100.0f;
  if (v < 0.0f) v = 0.0f;
  if (v > 1.0f) v = 1.0f;
  return FormatNumber(v);
}

std::string PassThrough(const std::string& value, const PropertyContext&, HTMLTransformContext&) {
  return Trim(value);
}

// ------------------------ Table population helpers -------------------------------------------

void AddKeep(std::unordered_map<std::string, PropertyHandler>& table, const char* name) {
  PropertyHandler h = {};
  h.action = PropAction::Keep;
  table[name] = h;
}

void AddTransform(std::unordered_map<std::string, PropertyHandler>& table, const char* name,
                  PropertyTransformFn fn) {
  PropertyHandler h = {};
  h.action = PropAction::Transform;
  h.transform = fn;
  table[name] = h;
}

void AddDrop(std::unordered_map<std::string, PropertyHandler>& table, const char* name,
             const char* msg) {
  PropertyHandler h = {};
  h.action = PropAction::Drop;
  h.dropMessage = msg;
  table[name] = h;
}

std::unordered_map<std::string, PropertyHandler> BuildTable() {
  std::unordered_map<std::string, PropertyHandler> t;

  // Layout
  AddTransform(t, "display", &TransformDisplay);
  AddTransform(t, "flex-direction", &TransformFlexDirection);
  AddTransform(t, "align-items", &TransformAlignItems);
  AddTransform(t, "justify-content", &TransformJustifyContent);
  AddTransform(t, "flex", &TransformFlex);
  AddTransform(t, "gap", &TransformLength);
  AddTransform(t, "padding", &TransformLengthShorthand);
  AddTransform(t, "padding-top", &TransformLength);
  AddTransform(t, "padding-right", &TransformLength);
  AddTransform(t, "padding-bottom", &TransformLength);
  AddTransform(t, "padding-left", &TransformLength);

  // Sizing
  AddTransform(t, "width", &TransformLength);
  AddTransform(t, "height", &TransformLength);
  AddKeep(t, "box-sizing");

  // Positioning
  AddTransform(t, "position", &TransformPosition);
  AddTransform(t, "left", &TransformLength);
  AddTransform(t, "right", &TransformLength);
  AddTransform(t, "top", &TransformLength);
  AddTransform(t, "bottom", &TransformLength);

  // Painting / effects
  AddKeep(t, "background-color");
  AddTransform(t, "background-image", &TransformBackgroundImage);
  AddTransform(t, "border", &TransformBorder);
  AddTransform(t, "border-radius", &TransformBorderRadius);
  AddKeep(t, "box-shadow");
  AddKeep(t, "filter");
  AddKeep(t, "backdrop-filter");
  AddTransform(t, "opacity", &TransformOpacity);
  AddKeep(t, "mix-blend-mode");
  AddKeep(t, "overflow");

  // Text
  AddKeep(t, "color");
  AddKeep(t, "font-family");
  AddTransform(t, "font-size", &TransformLength);
  AddKeep(t, "font-weight");
  AddKeep(t, "font-style");
  AddTransform(t, "letter-spacing", &TransformLength);
  AddTransform(t, "line-height", &PassThrough);  // px / unitless both accepted by importer
  AddKeep(t, "text-align");
  AddKeep(t, "text-decoration");
  AddKeep(t, "text-decoration-color");
  AddKeep(t, "white-space");
  AddKeep(t, "text-overflow");

  // Explicit drops -- recorded with rich diagnostics rather than the default "not in subset".
  AddDrop(t, "margin", "is not in the subset; use padding/gap/flex instead");
  AddDrop(t, "margin-top", "is not in the subset; use padding/gap/flex instead");
  AddDrop(t, "margin-right", "is not in the subset; use padding/gap/flex instead");
  AddDrop(t, "margin-bottom", "is not in the subset; use padding/gap/flex instead");
  AddDrop(t, "margin-left", "is not in the subset; use padding/gap/flex instead");
  AddDrop(t, "transform", "is not in the subset");
  AddDrop(t, "transform-origin", "is not in the subset");
  AddDrop(t, "perspective", "is not in the subset");
  AddDrop(t, "clip-path", "is not in the subset");
  AddDrop(t, "outline", "is not in the subset");
  AddDrop(t, "float", "is not in the subset");
  AddDrop(t, "order", "is not in the subset");
  AddDrop(t, "align-content", "is not in the subset");
  AddDrop(t, "align-self", "is not in the subset");
  AddDrop(t, "direction", "is not in the subset");
  AddDrop(t, "unicode-bidi", "is not in the subset");
  AddDrop(t, "flex-wrap", "is not in the subset");
  AddDrop(t, "flex-grow", "use 'flex: <N>' shorthand instead");
  // `flex-shrink` is handled as a transform because the canonical snapshot output
  // emits `flex-shrink: 0` on every flex item; treating it as a generic drop would
  // produce one warning per flex item with no actionable diagnostic.
  AddTransform(t, "flex-shrink", &TransformFlexShrink);
  AddDrop(t, "flex-basis", "use 'flex: <N>' shorthand instead");
  AddDrop(t, "min-width", "is not in the subset");
  AddDrop(t, "max-width", "is not in the subset");
  AddDrop(t, "min-height", "is not in the subset");
  AddDrop(t, "max-height", "is not in the subset");
  AddDrop(t, "aspect-ratio", "is not in the subset");
  AddDrop(t, "background-size", "is not in the subset");
  AddDrop(t, "background-repeat", "is not in the subset");
  AddDrop(t, "background-position", "is not in the subset");
  AddDrop(t, "text-transform", "is not in the subset");
  AddDrop(t, "text-indent", "is not in the subset");
  AddDrop(t, "word-spacing", "is not in the subset");
  AddDrop(t, "word-break", "is not in the subset");
  AddDrop(t, "overflow-wrap", "is not in the subset");
  AddDrop(t, "font-variant", "is not in the subset");
  AddDrop(t, "font-stretch", "is not in the subset");
  AddDrop(t, "font", "use the longhand properties instead");
  AddDrop(t, "grid-template-columns", "use 'display: flex' instead of grid");
  AddDrop(t, "grid-template-rows", "use 'display: flex' instead of grid");
  AddDrop(t, "grid-area", "use 'display: flex' instead of grid");
  AddDrop(t, "grid-column", "use 'display: flex' instead of grid");
  AddDrop(t, "grid-row", "use 'display: flex' instead of grid");
  AddDrop(t, "border-top", "use the shorthand 'border' instead");
  AddDrop(t, "border-right", "use the shorthand 'border' instead");
  AddDrop(t, "border-bottom", "use the shorthand 'border' instead");
  AddDrop(t, "border-left", "use the shorthand 'border' instead");
  AddDrop(t, "border-top-left-radius", "use the shorthand 'border-radius' instead");
  AddDrop(t, "border-top-right-radius", "use the shorthand 'border-radius' instead");
  AddDrop(t, "border-bottom-left-radius", "use the shorthand 'border-radius' instead");
  AddDrop(t, "border-bottom-right-radius", "use the shorthand 'border-radius' instead");
  AddDrop(t, "z-index", "is not in the subset");
  AddDrop(t, "cursor", "is not in the subset");
  AddDrop(t, "pointer-events", "is not in the subset");
  AddDrop(t, "user-select", "is not in the subset");
  AddDrop(t, "visibility", "is not in the subset");

  return t;
}

}  // namespace

const std::unordered_map<std::string, PropertyHandler>& SubsetPropertyTable() {
  static const std::unordered_map<std::string, PropertyHandler> table = BuildTable();
  return table;
}

std::string ResolveLength(const std::string& value, const PropertyContext& ctx,
                          HTMLTransformContext& diagnostics) {
  std::string trimmed = Trim(value);
  if (trimmed.empty()) return std::string();
  // `auto`, `none`, `inherit`, `initial`, `unset`, `revert`: drop silently (they're benign no-ops
  // when the subset uses explicit values everywhere).
  std::string lc = ToLower(trimmed);
  if (lc == "auto" || lc == "none" || lc == "inherit" || lc == "initial" || lc == "unset" ||
      lc == "revert") {
    return std::string();
  }
  if (lc.find("calc(") == 0) {
    return DropProperty(ctx.propertyName, value, "calc() is not supported by the subset",
                        diagnostics);
  }
  if (lc.find("var(") == 0) {
    return DropProperty(ctx.propertyName, value, "var() requires a Tailwind/CSS compiler pass",
                        diagnostics);
  }
  if (lc.find("clamp(") == 0 || lc.find("min(") == 0 || lc.find("max(") == 0) {
    return DropProperty(ctx.propertyName, value, "min/max/clamp() are not supported by the subset",
                        diagnostics);
  }
  float n = 0.0f;
  std::string unit;
  if (!ParseLengthComponent(trimmed, n, unit)) {
    return DropProperty(ctx.propertyName, value, "is not a recognised length", diagnostics);
  }
  unit = ToLower(unit);
  if (unit.empty() || unit == "px") {
    return EmitPx(n);
  }
  if (unit == "%") {
    return EmitPercent(n);
  }
  if (unit == "em") {
    float base = IsFiniteNonZero(ctx.currentFontSizePx) ? ctx.currentFontSizePx : kRemBasePx;
    diagnostics.warn("subset:unit-coerced", "html: '" + ctx.propertyName + ": " + trimmed +
                                                "' converted to " + FormatNumber(n * base) + "px");
    return EmitPx(n * base);
  }
  if (unit == "rem") {
    diagnostics.warn("subset:unit-coerced", "html: '" + ctx.propertyName + ": " + trimmed +
                                                "' converted to " + FormatNumber(n * kRemBasePx) +
                                                "px");
    return EmitPx(n * kRemBasePx);
  }
  if (unit == "vw" || unit == "vh") {
    float dim = unit == "vw" ? ctx.canvasWidth : ctx.canvasHeight;
    if (!IsFiniteNonZero(dim)) {
      return DropProperty(ctx.propertyName, value, "vw/vh require a known canvas size to resolve",
                          diagnostics);
    }
    float px = n * dim / 100.0f;
    diagnostics.warn("subset:unit-coerced", "html: '" + ctx.propertyName + ": " + trimmed +
                                                "' converted to " + FormatNumber(px) + "px");
    return EmitPx(px);
  }
  if (unit == "pt") {
    float px = n * 4.0f / 3.0f;
    diagnostics.warn("subset:unit-coerced", "html: '" + ctx.propertyName + ": " + trimmed +
                                                "' converted to " + FormatNumber(px) + "px");
    return EmitPx(px);
  }
  return DropProperty(ctx.propertyName, value, "uses an unsupported unit '" + unit + "'",
                      diagnostics);
}

std::string ResolveLengthShorthand(const std::string& value, const PropertyContext& ctx,
                                   HTMLTransformContext& diagnostics) {
  std::string trimmed = Trim(value);
  if (trimmed.empty()) return std::string();
  auto tokens = SplitTopLevelWhitespace(trimmed);
  if (tokens.empty()) return std::string();
  std::string out;
  out.reserve(trimmed.size());
  for (auto& t : tokens) {
    std::string resolved = ResolveLength(t, ctx, diagnostics);
    if (resolved.empty()) {
      continue;  // diagnostic already recorded by ResolveLength
    }
    if (!out.empty()) out.push_back(' ');
    out += resolved;
  }
  return out;
}

bool IsDataAttribute(const std::string& name) {
  return name.size() > 5 && name.compare(0, 5, "data-") == 0;
}

}  // namespace pagx::subset_props
