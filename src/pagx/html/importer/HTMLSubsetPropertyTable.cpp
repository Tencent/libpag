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

#include "pagx/html/importer/HTMLSubsetPropertyTable.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include "pagx/html/importer/HTMLDetail.h"

namespace pagx::html {

namespace {

constexpr float RemBasePx = 16.0f;

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
  return FormatRoundedNumber(v);
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

// `background-clip` only models the `text` keyword in PAGX. Combined with a gradient
// `background-image` it tells the importer to route the gradient onto descendant text fills
// instead of painting a rectangle. Other keywords (`border-box`, `padding-box`, …) are
// silent no-ops; only unknown values produce a diagnostic.
std::string TransformBackgroundClip(const std::string& value, const PropertyContext&,
                                    HTMLTransformContext& diags) {
  std::string lc = ToLower(Trim(value));
  if (lc.empty() || lc == "border-box") return std::string();
  if (lc == "text") return "text";
  if (lc == "padding-box" || lc == "content-box") return std::string();
  return DropProperty("background-clip", value, "is not a supported value", diags);
}

std::string TransformBorder(const std::string& value, const PropertyContext&,
                            HTMLTransformContext& diags) {
  std::string trimmed = Trim(value);
  if (trimmed.empty()) return std::string();
  std::string lc = ToLower(trimmed);
  if (lc == "none" || lc == "0" || lc == "0px") return std::string();
  // Accept the canonical `Wpx <style> <color>` form with arbitrary token order. `solid`,
  // `dashed`, and `dotted` are first-class and pass through to the resolver, which routes
  // them onto the Stroke node's dash pattern. Other style keywords (`double`, `groove`, …)
  // have no Stroke equivalent and are downgraded to `solid` here. Width/colour tokens pass
  // through unchanged.
  auto tokens = SplitTopLevelWhitespace(trimmed);
  for (auto& t : tokens) {
    std::string tl = ToLower(t);
    if (tl == "solid" || tl == "dashed" || tl == "dotted") continue;
    if (tl == "double" || tl == "groove" || tl == "ridge" || tl == "inset" || tl == "outset" ||
        tl == "none" || tl == "hidden") {
      diags.warn("subset:unsupported-property",
                 "html: 'border' style '" + tl + "' downgraded to 'solid'");
      continue;
    }
    float n = 0.0f;
    std::string unit;
    if (ParseLengthComponent(t, n, unit)) continue;
    // Otherwise assume color: pass through. The resolver's parseColor() emits a diagnostic
    // when a token is not a recognised colour, so we don't re-validate here.
  }
  return trimmed;
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

std::string TransformObjectFit(const std::string& value, const PropertyContext&,
                               HTMLTransformContext& diags) {
  std::string lc = ToLower(Trim(value));
  if (lc.empty()) return std::string();
  if (lc == "fill" || lc == "contain" || lc == "cover") return lc;
  if (lc == "none") {
    diags.warn("subset:unsupported-property",
               "html: 'object-fit: none' is not supported; downgraded to 'contain'");
    return "contain";
  }
  if (lc == "scale-down") {
    diags.warn("subset:unsupported-property",
               "html: 'object-fit: scale-down' is not supported; downgraded to 'contain'");
    return "contain";
  }
  return DropProperty("object-fit", value, "is not a recognised keyword", diags);
}

std::string PassThrough(const std::string& value, const PropertyContext&, HTMLTransformContext&) {
  return Trim(value);
}

// CSS `transform` is mapped onto the TextBox / Layer transform fields by
// HTMLStyleResolver. Single-function forms (skewX/skewY/rotate/scale[X|Y]/
// translate[X|Y]) populate the discrete fields and TextBox uses them
// directly. The `matrix(a, b, c, d, tx, ty)` shorthand is also accepted —
// the resolver writes the six floats straight into `HTMLTransform.matrix`
// so non-text Layers can forward the affine onto `Layer.matrix`. Compound
// chains like `skewX(-5deg) rotate(10deg)` and 3D variants
// (`matrix3d`/`rotate3d`/`perspective`) still require decomposition that
// the importer does not implement, so they are dropped with a warning.
std::string TransformTransform(const std::string& value, const PropertyContext&,
                               HTMLTransformContext& diags) {
  std::string trimmed = Trim(value);
  if (trimmed.empty()) return std::string();
  std::string lc = ToLower(trimmed);
  if (lc == "none") return std::string();
  size_t openParen = trimmed.find('(');
  if (openParen == std::string::npos || openParen == 0) {
    return DropProperty("transform", value, "is not a recognised function form", diags);
  }
  size_t closeParen = trimmed.find_last_of(')');
  if (closeParen == std::string::npos || closeParen <= openParen) {
    return DropProperty("transform", value, "is malformed", diags);
  }
  // The argument body lives between the FIRST `(` and its matching `)`. If anything other
  // than whitespace appears outside that span (either before the function name, between
  // the function call and the closing `)`, or after the closing `)`) the value is a
  // compound chain or contains nested calls — neither is supported in the single-function
  // subset, so drop with a diagnostic.
  size_t firstClose = trimmed.find(')');
  if (firstClose != closeParen) {
    return DropProperty("transform", value,
                        "compound transforms are not supported; use a single function "
                        "(skewX/skewY/rotate/scale/translate)",
                        diags);
  }
  std::string trailing = Trim(trimmed.substr(closeParen + 1));
  if (!trailing.empty()) {
    return DropProperty("transform", value,
                        "compound transforms are not supported; use a single function "
                        "(skewX/skewY/rotate/scale/translate)",
                        diags);
  }
  std::string fn = ToLower(Trim(trimmed.substr(0, openParen)));
  if (fn == "skewx" || fn == "skewy" || fn == "rotate" || fn == "scale" || fn == "scalex" ||
      fn == "scaley" || fn == "translate" || fn == "translatex" || fn == "translatey" ||
      fn == "matrix") {
    return trimmed;
  }
  return DropProperty("transform", value,
                      "is not in the supported function set "
                      "(skewX/skewY/rotate/scale[X|Y]/translate[X|Y]/matrix)",
                      diags);
}

// `transform-origin` is forwarded verbatim. The downstream resolver only
// understands the default "50% 50%" form (the typical inline output) and
// warns when it sees something else, so the table-level transform is a
// straight pass-through.
std::string TransformTransformOrigin(const std::string& value, const PropertyContext&,
                                     HTMLTransformContext&) {
  return Trim(value);
}

struct PropertyEntry {
  const char* name;
  PropAction action;
  PropertyTransformFn transform;  // non-null only when action == Transform
  const char* dropMessage;        // non-null only when action == Drop
};

// Static, data-driven property table. Adding a CSS property = adding one row here. Listed in
// roughly the same order as the resolver consumes them so it doubles as a quick subset map.
const PropertyEntry SubsetPropertyEntries[] = {
    // Layout
    {"display", PropAction::Transform, &TransformDisplay, nullptr},
    {"flex-direction", PropAction::Transform, &TransformFlexDirection, nullptr},
    {"align-items", PropAction::Transform, &TransformAlignItems, nullptr},
    {"justify-content", PropAction::Transform, &TransformJustifyContent, nullptr},
    {"flex", PropAction::Transform, &TransformFlex, nullptr},
    {"gap", PropAction::Transform, &ResolveLength, nullptr},
    {"padding", PropAction::Transform, &ResolveLengthShorthand, nullptr},
    {"padding-top", PropAction::Transform, &ResolveLength, nullptr},
    {"padding-right", PropAction::Transform, &ResolveLength, nullptr},
    {"padding-bottom", PropAction::Transform, &ResolveLength, nullptr},
    {"padding-left", PropAction::Transform, &ResolveLength, nullptr},
    // Sizing
    {"width", PropAction::Transform, &ResolveLength, nullptr},
    {"height", PropAction::Transform, &ResolveLength, nullptr},
    {"box-sizing", PropAction::Keep, nullptr, nullptr},
    // Positioning
    {"position", PropAction::Transform, &TransformPosition, nullptr},
    {"left", PropAction::Transform, &ResolveLength, nullptr},
    {"right", PropAction::Transform, &ResolveLength, nullptr},
    {"top", PropAction::Transform, &ResolveLength, nullptr},
    {"bottom", PropAction::Transform, &ResolveLength, nullptr},
    // Painting / effects
    {"background-color", PropAction::Keep, nullptr, nullptr},
    {"background-image", PropAction::Transform, &TransformBackgroundImage, nullptr},
    {"background-clip", PropAction::Transform, &TransformBackgroundClip, nullptr},
    {"border", PropAction::Transform, &TransformBorder, nullptr},
    // border-radius accepts 1-4 lengths or percentages; ResolveLengthShorthand handles both.
    {"border-radius", PropAction::Transform, &ResolveLengthShorthand, nullptr},
    {"box-shadow", PropAction::Keep, nullptr, nullptr},
    {"filter", PropAction::Keep, nullptr, nullptr},
    {"backdrop-filter", PropAction::Keep, nullptr, nullptr},
    {"opacity", PropAction::Transform, &TransformOpacity, nullptr},
    {"mix-blend-mode", PropAction::Keep, nullptr, nullptr},
    {"overflow", PropAction::Keep, nullptr, nullptr},
    {"object-fit", PropAction::Transform, &TransformObjectFit, nullptr},
    // Text
    {"color", PropAction::Keep, nullptr, nullptr},
    {"font-family", PropAction::Keep, nullptr, nullptr},
    {"font-size", PropAction::Transform, &ResolveLength, nullptr},
    {"font-weight", PropAction::Keep, nullptr, nullptr},
    {"font-style", PropAction::Keep, nullptr, nullptr},
    {"letter-spacing", PropAction::Transform, &ResolveLength, nullptr},
    {"line-height", PropAction::Transform, &PassThrough, nullptr},  // px / unitless both accepted
    {"text-align", PropAction::Keep, nullptr, nullptr},
    {"text-decoration", PropAction::Keep, nullptr, nullptr},
    {"text-decoration-color", PropAction::Keep, nullptr, nullptr},
    {"white-space", PropAction::Keep, nullptr, nullptr},
    {"text-overflow", PropAction::Keep, nullptr, nullptr},
    {"writing-mode", PropAction::Keep, nullptr, nullptr},
    // `transform` and `transform-origin` are forwarded so HTMLStyleResolver can map the
    // function back onto the TextBox / Group transform fields. The handler enforces the
    // supported single-function subset; compound chains and matrix(...) forms are dropped.
    {"transform", PropAction::Transform, &TransformTransform, nullptr},
    {"transform-origin", PropAction::Transform, &TransformTransformOrigin, nullptr},
    // `flex-shrink` is handled as a transform because the canonical snapshot output emits
    // `flex-shrink: 0` on every flex item; treating it as a generic drop would produce one
    // warning per flex item with no actionable diagnostic.
    {"flex-shrink", PropAction::Transform, &TransformFlexShrink, nullptr},
    // Explicit drops -- recorded with rich diagnostics rather than the default "not in subset".
    {"margin", PropAction::Drop, nullptr, "is not in the subset; use padding/gap/flex instead"},
    {"margin-top", PropAction::Drop, nullptr, "is not in the subset; use padding/gap/flex instead"},
    {"margin-right", PropAction::Drop, nullptr,
     "is not in the subset; use padding/gap/flex instead"},
    {"margin-bottom", PropAction::Drop, nullptr,
     "is not in the subset; use padding/gap/flex instead"},
    {"margin-left", PropAction::Drop, nullptr,
     "is not in the subset; use padding/gap/flex instead"},
    {"perspective", PropAction::Drop, nullptr, "is not in the subset"},
    {"clip-path", PropAction::Drop, nullptr, "is not in the subset"},
    {"outline", PropAction::Drop, nullptr, "is not in the subset"},
    {"float", PropAction::Drop, nullptr, "is not in the subset"},
    {"order", PropAction::Drop, nullptr, "is not in the subset"},
    {"align-content", PropAction::Drop, nullptr, "is not in the subset"},
    {"align-self", PropAction::Drop, nullptr, "is not in the subset"},
    {"direction", PropAction::Drop, nullptr, "is not in the subset"},
    {"unicode-bidi", PropAction::Drop, nullptr, "is not in the subset"},
    {"flex-wrap", PropAction::Drop, nullptr, "is not in the subset"},
    {"flex-grow", PropAction::Drop, nullptr, "use 'flex: <N>' shorthand instead"},
    {"flex-basis", PropAction::Drop, nullptr, "use 'flex: <N>' shorthand instead"},
    {"min-width", PropAction::Drop, nullptr, "is not in the subset"},
    {"max-width", PropAction::Drop, nullptr, "is not in the subset"},
    {"min-height", PropAction::Drop, nullptr, "is not in the subset"},
    {"max-height", PropAction::Drop, nullptr, "is not in the subset"},
    {"aspect-ratio", PropAction::Drop, nullptr, "is not in the subset"},
    {"background-size", PropAction::Drop, nullptr, "is not in the subset"},
    {"background-repeat", PropAction::Drop, nullptr, "is not in the subset"},
    {"background-position", PropAction::Drop, nullptr, "is not in the subset"},
    {"text-transform", PropAction::Drop, nullptr, "is not in the subset"},
    {"text-indent", PropAction::Drop, nullptr, "is not in the subset"},
    {"word-spacing", PropAction::Drop, nullptr, "is not in the subset"},
    {"word-break", PropAction::Drop, nullptr, "is not in the subset"},
    {"overflow-wrap", PropAction::Drop, nullptr, "is not in the subset"},
    {"font-variant", PropAction::Drop, nullptr, "is not in the subset"},
    {"font-stretch", PropAction::Drop, nullptr, "is not in the subset"},
    {"font", PropAction::Drop, nullptr, "use the longhand properties instead"},
    {"grid-template-columns", PropAction::Drop, nullptr, "use 'display: flex' instead of grid"},
    {"grid-template-rows", PropAction::Drop, nullptr, "use 'display: flex' instead of grid"},
    {"grid-area", PropAction::Drop, nullptr, "use 'display: flex' instead of grid"},
    {"grid-column", PropAction::Drop, nullptr, "use 'display: flex' instead of grid"},
    {"grid-row", PropAction::Drop, nullptr, "use 'display: flex' instead of grid"},
    {"border-top", PropAction::Drop, nullptr, "use the shorthand 'border' instead"},
    {"border-right", PropAction::Drop, nullptr, "use the shorthand 'border' instead"},
    {"border-bottom", PropAction::Drop, nullptr, "use the shorthand 'border' instead"},
    {"border-left", PropAction::Drop, nullptr, "use the shorthand 'border' instead"},
    {"border-top-left-radius", PropAction::Drop, nullptr,
     "use the shorthand 'border-radius' instead"},
    {"border-top-right-radius", PropAction::Drop, nullptr,
     "use the shorthand 'border-radius' instead"},
    {"border-bottom-left-radius", PropAction::Drop, nullptr,
     "use the shorthand 'border-radius' instead"},
    {"border-bottom-right-radius", PropAction::Drop, nullptr,
     "use the shorthand 'border-radius' instead"},
    {"z-index", PropAction::Drop, nullptr, "is not in the subset"},
    {"cursor", PropAction::Drop, nullptr, "is not in the subset"},
    {"pointer-events", PropAction::Drop, nullptr, "is not in the subset"},
    {"user-select", PropAction::Drop, nullptr, "is not in the subset"},
    {"visibility", PropAction::Drop, nullptr, "is not in the subset"},
};

std::unordered_map<std::string, PropertyHandler> BuildTable() {
  std::unordered_map<std::string, PropertyHandler> t;
  t.reserve(sizeof(SubsetPropertyEntries) / sizeof(SubsetPropertyEntries[0]));
  for (const auto& entry : SubsetPropertyEntries) {
    PropertyHandler h = {};
    h.action = entry.action;
    h.transform = entry.transform;
    h.dropMessage = entry.dropMessage;
    t[entry.name] = h;
  }
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
  // Delegate em / rem / pt / vw / vh / unknown unit handling to the shared helper so
  // ResolveLength and parsePxLength agree on numeric semantics. ResolveLength keeps the
  // richer diagnostics here (the helper just answers "what px does this resolve to?").
  bool recognized = false;
  float fontBase = IsFiniteNonZero(ctx.currentFontSizePx) ? ctx.currentFontSizePx : RemBasePx;
  float px = ConvertCssLengthToPx(n, unit, fontBase, ctx.canvasWidth, ctx.canvasHeight, recognized);
  if (recognized) {
    diagnostics.warn("subset:unit-coerced", "html: '" + ctx.propertyName + ": " + trimmed +
                                                "' converted to " + FormatNumber(px) + "px");
    return EmitPx(px);
  }
  if (unit == "vw" || unit == "vh") {
    return DropProperty(ctx.propertyName, value, "vw/vh require a known canvas size to resolve",
                        diagnostics);
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

}  // namespace pagx::html
