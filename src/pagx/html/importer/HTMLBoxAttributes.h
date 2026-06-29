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
#include "pagx/types/Padding.h"

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
  std::string color = {};
  std::string fontFamily = {};
  std::string fontSize = {};
  std::string fontWeight = {};
  std::string fontStyle = {};
  std::string fontStyleName = {};  // real-face style label, e.g. "Light" / "Medium" / ""
  // Synthetic weight / slant the renderer must emboss on top of the resolved face. Set by
  // `resolveInheritedStyle` for bold (CSS weight >= 600) and italic/oblique requests whose axis is
  // dropped from `fontStyleName` (see `ResolveFontStyleSynthesis`). Carried through to
  // `Text::fauxBold` / `Text::fauxItalic` so authored weight / slant survives even when the styled
  // web face is not installed on the render host.
  bool fauxBold = false;
  bool fauxItalic = false;
  std::string letterSpacing = {};
  std::string lineHeight = {};
  std::string textAlign = {};
  std::string textDecoration = {};
  std::string textDecorationColor = {};
  std::string whiteSpace = {};
  std::string writingMode = {};
  // Gradient string ("linear-gradient(...)" / "radial-gradient(...)" / "conic-gradient(...)")
  // inherited from the nearest ancestor that combined `background-clip: text` with a gradient
  // `background-image`. Empty means descendants paint text with their own solid `color`.
  std::string textFillImage = {};

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
 * Resolved CSS `transform` for a single HTML element. Populated when the
 * `transform` declaration uses one of the supported single-function forms
 * (skewX/skewY/rotate/scale[X|Y]/translate[X|Y]) or the `matrix(a, b, c, d,
 * tx, ty)` shorthand. Compound function chains and 3D variants
 * (`matrix3d`/`rotate3d`/`perspective`/…) are dropped earlier in the subset
 * transformer.
 *
 * The single-function variants populate the discrete fields (skew/rotation/
 * scale/translate) so the existing TextBox path can map them straight onto
 * `TextBox.skew/skewAxis/rotation/scale`. ALL forms — including the
 * compound ones reached only through `matrix(...)` — additionally fill
 * `matrix` with the equivalent affine, so the new generic Layer path can
 * write it onto `Layer.matrix` regardless of the source CSS function used.
 * `valid` is false when no usable transform was set; in that case `matrix`
 * stays at the identity and the discrete fields keep their defaults
 * (skew/rotation/translate = 0; scale = 1).
 */
struct HTMLTransform {
  bool valid = false;
  float skew = 0.0f;        // pagx skew (deg); CSS skewX(α) -> -α; CSS skewY(α) -> α.
  float skewAxis = 0.0f;    // pagx skewAxis (deg); 0 for skewX, 90 for skewY.
  float rotation = 0.0f;    // pagx rotation (deg); from CSS rotate(α).
  float scaleX = 1.0f;      // pagx scale.x; from CSS scale*().
  float scaleY = 1.0f;      // pagx scale.y; from CSS scale*().
  float translateX = 0.0f;  // CSS translate*().x in px; folded into Layer left/top.
  float translateY = 0.0f;  // CSS translate*().y in px; folded into Layer left/top.
  // Affine matrix equivalent of the parsed CSS transform. Single-function
  // forms compose this from their discrete arguments; the `matrix(...)`
  // form populates this directly. The matrix here is the *raw* CSS value;
  // `transform-origin` is NOT folded in. Callers that consume the matrix on
  // a non-text Layer must apply the origin themselves (typically `T(cx,cy)
  // * matrix * T(-cx,-cy)` for the canonical `50% 50%` origin).
  Matrix matrix = {};
  // CSS `transform-origin` resolved to absolute pixels relative to the
  // element's box. Both default to NaN, meaning "use the box's geometric
  // centre" (the CSS default of `50% 50%` and the only origin Chromium
  // emits when no source CSS overrides it). When the source CSS specifies
  // a different origin, parseBoxTransform writes the resolved px values
  // here so applyBoxTransform pivots around the requested point instead
  // of the centre.
  float originXPx = NAN;
  float originYPx = NAN;
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
  bool flexRow = true;  // default of CSS flex
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

  // CSS `clip-path: url(#id)` reference (raw, including the `url(...)` wrapper and any quotes).
  // The importer resolves the referenced hidden `<clipPath>` def into a contour mask layer (the
  // inverse of `HTMLWriter::writeClipDef`). Empty means "no clip-path authored".
  std::string clipPathRef = {};

  float opacity = 1.0f;
  bool opacitySet = false;

  std::string mixBlendMode = {};

  bool clipOverflow = false;

  // Lower-cased CSS `object-fit` keyword for replaced elements (currently only
  // `<img>`). Empty means "not set" and should fall back to the CSS default
  // `fill`, which corresponds to PAGX `ScaleMode::Stretch`.
  std::string objectFit = {};

  // Inline `transform` mapped to the TextBox / Group transform fields. Currently the
  // importer only honours `valid == true` for text leaves (where the resolved fields
  // get written onto the synthesised TextBox); on non-text elements the transform is
  // dropped with a diagnostic at apply time because Layer has no skew/rotation fields.
  HTMLTransform transform = {};
};

}  // namespace pagx
