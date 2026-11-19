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

#include <cmath>
#include <string>
#include <vector>
#include "pagx/types/Color.h"
#include "pagx/types/ColorSpace.h"
#include "pagx/types/Matrix.h"
#include "pagx/types/Matrix3D.h"
#include "pagx/types/Padding.h"
#include "pagx/types/Point.h"

namespace pagx {

/**
 * Default CSS font-size used for HTML text leaves before any user style is applied.
 */
static constexpr float HTML_DEFAULT_FONT_SIZE = 14.0f;

/**
 * Default font family used when no `font-family` is inherited.
 */
static constexpr const char* HTML_DEFAULT_FONT_FAMILY = "Arial";

/**
 * Default font style/variant name written to every imported `Text` node. The synthesis in
 * `ResolveFontStyleSynthesis` leaves the style label empty for the plain Regular-weight upright
 * face; this constant substitutes the canonical "Regular" name so every HTML-imported `Text`
 * node always carries a concrete `fontStyle`.
 */
static constexpr const char* HTML_DEFAULT_FONT_STYLE = "Regular";

/** Returns a concrete PAGX font style name for a resolved HTML face. */
inline std::string ResolveHTMLFontStyleName(const std::string& fontStyleName) {
  return fontStyleName.empty() ? HTML_DEFAULT_FONT_STYLE : fontStyleName;
}

/**
 * Default text colour used when no `color` is inherited; matches the CSS default for
 * <body> in `ElementDefaults()`.
 */
static constexpr const char* HTML_DEFAULT_TEXT_COLOR = "#1E293B";

/**
 * Pixel tolerance used when comparing image and wrapper geometry for the rounded-image
 * fold optimisation in `foldRoundedImageWrapper`.
 */
static constexpr float HTML_IMAGE_WRAPPER_TOLERANCE_PX = 0.5f;

/**
 * Inherited CSS style properties that cascade down the element tree during HTML
 * traversal. Empty strings denote "not set" (preserved so that descendants can know
 * whether to inherit further or use their own defaults).
 */
struct HTMLInheritedStyle {
  // CSS `perspective` declared on the *parent* element (px), pushed into this element's pose. Not
  // inherited: `resolveInheritedStyle` resets it, and a parent sets it on the bag it hands to its
  // direct children only. The origin is the parent's `perspective-origin` in the parent's local
  // coordinates; the child subtracts its own offset to express it locally.
  float parentPerspectivePx = 0.0f;
  float parentPerspectiveOriginX = NAN;
  float parentPerspectiveOriginY = NAN;
  // The parent's border width: `perspective-origin` is measured in the parent's border box, while an
  // absolutely positioned child's `left` / `top` start at its padding box, so the offset needs this.
  float parentBorderWidthPx = 0.0f;
  std::string color = {};
  std::string fontFamily = {};
  std::string fontSize = {};
  std::string fontWeight = {};
  std::string fontStyle = {};
  std::string fontStyleName = {};  // real-face style label, e.g. "Light" / "Bold" / "Italic" / ""
  // Synthetic slant the renderer must emboss on top of the resolved face. Set by
  // `resolveInheritedStyle` for italic/oblique requests and carried through to `Text::fauxItalic`
  // so the authored slant survives even when the styled italic face is not installed on the
  // render host. The face label itself keeps the italic axis (`fontStyleName` may be "Italic" or
  // "Medium Italic"); text layout drops this flag when the resolved typeface already provides a
  // real italic face, so the slant is never applied twice. The importer never pre-synthesises the
  // weight axis: it stays in `fontStyleName` as a real-face keyword, so `fauxBold` remains false
  // even if font lookup later falls back to a lighter face.
  bool fauxBold = false;
  bool fauxItalic = false;
  std::string letterSpacing = {};
  std::string lineHeight = {};
  std::string textAlign = {};
  std::string textDecoration = {};
  std::string textDecorationColor = {};
  std::string whiteSpace = {};
  std::string writingMode = {};
  // CSS `-webkit-text-stroke` resolved by HTMLStyleCascade into a stroke width (px) and a
  // concrete colour. `textStrokeWidthPx` is NaN when no stroke is authored (or the width is
  // zero, which paints nothing); a positive value drives an extra PAGX text `<Stroke>` emitted
  // after the Fill (the inverse of HTMLWriter's `-webkit-text-stroke` emission). The colour
  // defaults to the resolved text colour when the shorthand omits it, matching CSS.
  float textStrokeWidthPx = NAN;
  Color textStrokeColor = {0, 0, 0, 1, ColorSpace::SRGB};
  // Gradient string ("linear-gradient(...)" / "radial-gradient(...)" / "conic-gradient(...)")
  // inherited from the nearest ancestor that combined `background-clip: text` with a gradient
  // `background-image`. Empty means descendants paint text with their own solid `color`.
  std::string textFillImage = {};
  // Solid colour inherited from the nearest ancestor that combined `background-clip: text`
  // with a solid `background-color` (the same CSS technique as `textFillImage`, without a
  // gradient layer). In CSS the background clipped to the glyphs is what paints the text, so
  // this outranks `color`; `textFillSolidSet` stays false when no such ancestor exists and
  // descendants keep painting with their own resolved `color`.
  Color textFillSolid = {0, 0, 0, 1, ColorSpace::SRGB};
  bool textFillSolidSet = false;

  // Pre-resolved numeric forms of the cascade. Kept in lock-step with the string fields by
  // `resolveInheritedStyle` so text-leaf conversion doesn't re-parse the same `font-size` /
  // `letter-spacing` / `color` strings for every fragment.
  float fontSizePx = HTML_DEFAULT_FONT_SIZE;
  float letterSpacingPx = 0.0f;
  Color resolvedTextColor = {0, 0, 0, 1, ColorSpace::SRGB};

  // Pre-resolved CSS font-family stack. Kept in lock-step with `fontFamily` by
  // `resolveInheritedStyle`. `primaryFontFamily` is the first concrete name (after surrounding
  // quotes are stripped and generic keywords are mapped to platform fonts) and gets
  // written to `Text::fontFamily`. `fontFamilyChain` holds the same first name followed by
  // every additional concrete name from the stack, in CSS order; the importer unions all
  // chains across the document and registers the union as user fallback fonts on the
  // document's FontConfig so per-glyph fallback in LayoutContext can pick them up.
  std::string primaryFontFamily = {};
  std::vector<std::string> fontFamilyChain = {};
};

/**
 * Resolved CSS `transform` for a single HTML element: the whole function list composed in CSS order
 * into a 4x4 pose, plus the resolved `transform-origin`. The pivot is never folded into `pose`;
 * `applyBoxTransform` writes it onto `Layer.anchor` so matrix / matrix3D keyframes stay un-pivoted.
 *
 * `valid` is false when the declaration is absent, `none`, or was rejected by the parser (which
 * reports the reason). `originResolved` is false when a percentage / keyword origin could not be
 * resolved because the reference box has no absolute size; the caller then pivots at the top-left.
 */
struct HTMLTransform {
  bool valid = false;
  Matrix3D pose = {};
  Point origin = {};
  float originZ = 0.0f;
  bool originResolved = false;
};

/**
 * CSS border line styles supported by the HTML subset. Mirrors a subset of
 * `border-style` keywords; other styles (`double`, `groove`, …) are downgraded
 * to `Solid` by the resolver.
 */
enum class BorderStyle {
  Solid,
  Dashed,
  Dotted,
};

/**
 * Resolved box-model attributes for a single HTML element. Anything left as NaN / empty
 * is "not specified" and falls back to PAGX defaults (which mirror CSS defaults for the
 * accepted subset).
 */
struct HTMLBoxAttributes {
  // Sizing
  float widthPx = NAN;    // explicit px width
  float widthPct = NAN;   // explicit % width (0-100)
  float heightPx = NAN;   // explicit px height
  float heightPct = NAN;  // explicit % height (0-100)

  // Positioning (only when position: absolute)
  bool absolute = false;
  float leftPx = NAN;
  float rightPx = NAN;
  float topPx = NAN;
  float bottomPx = NAN;

  // Layout
  bool displayFlex = false;
  // True when the flex main axis is geometrically horizontal. Starts from CSS `flex-direction`
  // (row → true) and is then rotated by the element's writing-mode in `parseBoxLayout`: a vertical
  // writing mode flips the inline/block axes, so `flex-direction: column` under `vertical-rl/lr`
  // ends up horizontal. Maps directly to LayoutMode (Horizontal when true, Vertical otherwise).
  bool flexRow = true;
  float gapPx = 0.0f;
  bool gapSet = false;
  Padding padding = {};
  bool paddingSet = false;
  std::string alignItems = {};
  std::string justifyContent = {};
  float flexGrow = 0.0f;
  bool flexGrowSet = false;

  // CSS `margin` (outer offsets, not size) resolved to pixels per side. Defaults to 0 so the
  // common "no margin authored" case forwards through the importer unchanged. PAGX has no
  // margin concept on Layer / LayoutNode, so the importer materialises margin entirely
  // through positioning + padding at apply time:
  //   - position: absolute → folded directly into the matching edge anchor
  //     (left += marginLeft, top += marginTop, right += marginRight, bottom += marginBottom);
  //   - flow / flex children → wrapped in an outer Layer whose `padding` equals the margin,
  //     reproducing CSS's "outer size = inner size + margin" measurement contract for the
  //     parent flex / constraint pass.
  // See `HTMLParserContext::wrapForMargin` for the apply-side logic.
  float marginTopPx = 0.0f;
  float marginRightPx = 0.0f;
  float marginBottomPx = 0.0f;
  float marginLeftPx = 0.0f;

  // Effects
  Color backgroundColor = {0, 0, 0, 0, ColorSpace::SRGB};
  bool backgroundColorSet = false;
  std::string backgroundImage = {};
  // True when computed style had `background-clip: text`. Only meaningful with a gradient
  // `background-image`; the gradient is then routed onto descendant text fills instead of
  // painting a rectangle on this element.
  bool backgroundClipText = false;
  // Per-layer `background-clip` box keywords (`border-box` / `padding-box`), comma-separated
  // in CSS layer order (first entry clips the top-most background-image layer, matching
  // `backgroundImage`'s order). Empty means every layer clips to the default `border-box`.
  // A `padding-box` layer is how CSS paints gradient borders: the layer inset by the border
  // width lets the border-box layer beneath show through as a frame.
  std::string backgroundClip = {};

  // CSS `background-size` / `background-repeat` / `background-position`, kept lower-cased and
  // trimmed. Only meaningful when `backgroundImage` is a `url(...)` reference; the importer maps
  // them back onto an `ImagePattern` fill — the inverse of `HTMLWriter`'s exporter emission:
  //   - size `contain` / `cover` / `100% 100%` → ScaleMode LetterBox / Zoom / Stretch (a fitted
  //     mode that centres the image and ignores repeat/position).
  //   - any other case (explicit `<w>px <h>px` size, or `repeat`) → ScaleMode None with the
  //     pattern matrix carrying the per-axis scale (tile px / native px) and the position offset,
  //     and tile modes taken from `background-repeat`.
  // Empty means "not authored" and the property falls back to its CSS default.
  std::string backgroundSize = {};
  std::string backgroundRepeat = {};
  std::string backgroundPosition = {};

  // CSS `background-blend-mode`, lower-cased and trimmed. Blends the element's background
  // layers (gradient / image over background-color) among themselves. A non-empty, non-`normal`
  // value drives the gradient/image Fill's `blendMode`; the importer then also keeps the
  // background-color Fill underneath so the blend composites against a backdrop. Empty (or
  // `normal`) means "no blending" and the background paints with its default opaque stacking.
  std::string backgroundBlendMode = {};

  // CSS `border-radius` expanded to four corners (TL, TR, BR, BL) in pixels, after applying the
  // CSS "edge overlap" scaling clamp (radii are shrunk uniformly so adjacent corner pairs never
  // exceed the box's edge length). When the input was uniform — or all four resolved values are
  // equal after clamping — `borderRadiusUniform` is true and the importer emits a single
  // `Rectangle roundness=...`; otherwise it synthesises a `Path` tracing the per-corner outline.
  float borderRadiusTLPx = 0.0f;
  float borderRadiusTRPx = 0.0f;
  float borderRadiusBRPx = 0.0f;
  float borderRadiusBLPx = 0.0f;
  bool borderRadiusSet = false;
  bool borderRadiusUniform = true;
  // True when every corner's horizontal radius equals half the box width and every corner's
  // vertical radius equals half the box height — the exact condition under which a CSS
  // rounded rectangle degenerates into an ellipse inscribed in the box (`border-radius: 50%`
  // being the canonical author syntax). The importer then emits a PAGX `Ellipse` instead of a
  // `Rectangle`, so a non-square box renders a true ellipse rather than a pill approximation.
  bool borderRadiusEllipse = false;

  float borderWidthPx = 0.0f;
  Color borderColor = {0, 0, 0, 1, ColorSpace::SRGB};
  BorderStyle borderStyle = BorderStyle::Solid;
  bool borderSet = false;

  std::string boxShadow = {};
  std::string filter = {};
  std::string backdropFilter = {};

  // CSS `mask-image` / `mask-mode` / `mask-size` / `mask-position` for alpha and luminance masks.
  // `maskImage` is the raw `url(data:image/svg+xml,...)` the HTML exporter emitted; the importer
  // decodes the embedded SVG and rebuilds a PAGX mask layer from it (the inverse of
  // `HTMLWriter::writeMaskCSS`). `maskMode` is the lower-cased keyword (`alpha` / `luminance`);
  // `maskSize` / `maskPosition` are lower-cased and trimmed and drive the mask layer's scale /
  // offset. All empty means "no mask authored".
  std::string maskImage = {};
  std::string maskMode = {};
  std::string maskSize = {};
  std::string maskPosition = {};

  // CSS `clip-path` value, kept raw. Either a `url(#id)` reference to a hidden `<clipPath>` def
  // (including the `url(...)` wrapper and any quotes) or a CSS basic shape (`polygon()` / `path()`
  // / `circle()` / `ellipse()` / `inset()`). The importer rebuilds both into a contour mask layer
  // (the `url` form is the inverse of `HTMLWriter::writeClipDef`). Empty means "no clip-path".
  std::string clipPathRef = {};

  float opacity = 1.0f;
  bool opacitySet = false;

  std::string mixBlendMode = {};

  bool clipOverflow = false;

  // Lower-cased CSS `object-fit` keyword for replaced elements (currently only
  // `<img>`). Empty means "not set" and should fall back to the CSS default
  // `fill`, which corresponds to PAGX `ScaleMode::Stretch`.
  std::string objectFit = {};

  // CSS `transform-style: preserve-3d`, forwarded onto `Layer.preserve3D`.
  bool preserve3D = false;

  // CSS `perspective` declared on the parent (px), folded into this element's pose. Stored on the box
  // so the transform write and the animation registration read the same value regardless of which
  // element happens to be converting when they run. `localPerspectiveOrigin*` is the parent's
  // `perspective-origin` re-expressed in this element's local coordinates.
  float parentPerspectivePx = 0.0f;
  float localPerspectiveOriginX = NAN;
  float localPerspectiveOriginY = NAN;

  // CSS `perspective: <d>` on this element: a camera that projects its **children**. PAGX keeps the
  // projection on the layer that owns the pose, so the parent value is pushed into each child pose
  // as `P * pose`, where `P` is the identity with bottom row `(0, 0, -1/d, 1)`. 0 means "no
  // perspective".
  float perspectivePx = 0.0f;
  // CSS `perspective-origin` of the camera above, in this box's local pixels (default: the centre).
  float perspectiveOriginX = NAN;
  float perspectiveOriginY = NAN;

  HTMLTransform transform = {};
};

}  // namespace pagx
