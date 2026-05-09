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
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "pagx/html/FontHoist.h"
#include "pagx/html/HTMLBuilder.h"
#include "pagx/html/HTMLPlusDarkerRenderer.h"
#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/RangeSelector.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/types/Alignment.h"
#include "pagx/types/Arrangement.h"
#include "pagx/types/MergePathMode.h"
#include "pagx/types/Padding.h"
#include "pagx/types/Rect.h"
#include "pagx/types/SelectorTypes.h"

namespace pagx {

class Group;
class Polystar;
class Repeater;
class Text;
class TextBox;
class TextModifier;
class TextPath;
class TrimPath;

// Kappa value for 90-degree bezier arc approximation: 4 * (sqrt(2) - 1) / 3
static constexpr float kBezierKappa = 0.5522847498307936f;

//==============================================================================
// Shared static utility functions
//==============================================================================

void ColorToRGB(const Color& color, int& r, int& g, int& b);
std::string ColorToHex(const Color& color);
std::string ColorToSVGHex(const Color& color);
std::string ColorToRGBA(const Color& color, float extra = 1.0f);
std::string CSSStops(const std::vector<ColorStop*>& stops);
std::string MatrixToCSS(const Matrix& m);
std::string MatrixTransformToCSS(const Matrix& m);
std::string Matrix3DToCSS(const Matrix3D& m);
const char* BlendModeToMixBlendMode(BlendMode mode);
Color LerpColor(const Color& a, const Color& b, float t);

/**
 * Samples a color from a gradient at normalized position t ([0, 1]) along the gradient line,
 * clamped at the first and last stops. Used by TextModifier rendering to compute a per-character
 * solid color based on each character's position within the overall text span's gradient.
 */
Color SampleLinearGradient(const std::vector<ColorStop*>& stops, float t);

std::string LayerTransformCSS(const Layer* layer);

/**
 * Rewrites line-break-hint characters inside a Text node's literal text so the browser's CSS
 * line-breaker matches PAGX's UAX-14 behaviour without changing the visible text:
 *
 *  - Replaces SHY (U+00AD) with ZWSP (U+200B). PAGX treats SHY as an invisible break-allowed
 *    point, but Chromium follows the CSS spec and renders SHY as a visible hyphen at the line
 *    end — replacing it with ZWSP keeps the same break-allowed semantics with no visible glyph.
 *  - Inserts ZWSP after every '/' (slash). PAGX's UAX-14 maps '/' to class BA (break-allowed),
 *    but Chromium's `word-wrap:break-word` does not break at '/' inside a single token like
 *    "path/to/some/file" and falls back to mid-character splitting. The injected ZWSP is the
 *    standard web-typography hint that lets Chromium break right after the '/' the same way
 *    tgfx does.
 *
 * Both characters are zero-width to the user and only affect line-break decisions. Other
 * line-break-affecting Unicode characters (ZWSP itself, WJ U+2060, IN/OP/CL/NS/QU classes
 * such as 「」、——、…、 ー、'") render the same way in Chromium and tgfx without rewriting,
 * so they are passed through unchanged. Returns the rewritten string.
 */
std::string RewriteLineBreakHints(const std::string& text);

Matrix BuildGroupMatrix(const Group* group);
const char* AlignmentToCSS(Alignment alignment);
const char* ArrangementToCSS(Arrangement arrangement);
std::string PaddingToCSS(const Padding& padding);

/**
 * Returns true if the text content's first strong directional character is RTL (Hebrew, Arabic,
 * etc.). Used to emit `direction:rtl` on TextBox containers so the browser's Unicode BiDi
 * algorithm aligns paragraphs to the right, matching PAGX's tgfx layout behaviour.
 */
bool TextStartsWithRTL(const std::string& utf8Text);

/**
 * Builds inline HTML content for a vertical-writing-mode Text that needs per-character
 * spacing control (typically `textAlign="justify"` with a declared `lineHeight`). The
 * algorithm walks the source text in lockstep with the tgfx-produced layoutRuns positions,
 * wraps each CJK character (or individual upright glyph) in an `<span style="display:inline-
 * block;height:{advance}px">` so its inline-axis advance matches tgfx exactly, and keeps
 * contiguous Latin/digit runs wrapped as a single `<span>` so they retain their natural
 * horizontal shaping. Newlines (U+000A) become `<br>`. Returns an HTML fragment whose
 * characters are already HTML-escaped and that must be emitted inside an outer `<span>`
 * carrying font/color styling via `closeTagWithRawContent`.
 *
 * Falls back to an empty string (caller should fall back to the plain text path) when the
 * text has no associated layoutRuns or when the glyph/char counts diverge.
 */
std::string BuildVerticalJustifyContent(const Text* text);

std::string GetImageSrc(const Image* image);
const char* DetectImageMime(const uint8_t* bytes, size_t size);

/**
 * Decodes just the header of the given image to recover its native pixel dimensions. Returns
 * {0, 0} when the image is missing, unreadable, or in a format tgfx cannot decode. Used by the
 * CSS ImagePattern branch to emit `background-size` in absolute pixels so repeat tiling matches
 * tgfx's texture-coordinate transform exactly.
 */
std::pair<int, int> GetImageNativeSize(const Image* image);

std::string EscapeCSSUrl(const std::string& url);

std::string BuildPolystarPath(const Polystar* ps);
std::string GetPathSVGString(const Path* path);
std::string ReversePathDataToSVGString(const PathData& pathData);
std::string RectToPathData(const Rectangle* r);
std::string EllipseToPathData(const Ellipse* e);
void GeoToPathData(const Element* element, NodeType type, PathData& output);
void ReversePathData(const PathData& pathData, PathData& output);
void ApplyRoundCorner(const PathData& pathData, float radius, PathData& output);
std::string TransformPathDataToSVG(const PathData& pathData, const Matrix& m);

// Returns the transform matrix of the i-th copy produced by a Repeater, using the same formula
// writeRepeater applies per copy: anchor recenter + scale^prog + rotate*prog + position*prog.
// `i` is the raw loop index; the order (AboveOriginal vs BelowOriginal) and offset are folded in.
Matrix BuildRepeaterCopyMatrix(const Repeater* rep, int i);

float ComputePathLength(const PathData& pathData);
float ComputePathSignedArea(const PathData& pathData);
bool IsPathClockwise(const PathData& pathData);
float ComputeCubicBezierLength(Point p0, Point p1, Point p2, Point p3, int depth = 0);
float ComputeQuadBezierLength(Point p0, Point p1, Point p2);

// Text helpers
bool IsCJKCodepoint(int32_t ch);
float EstimateCharAdvanceHTML(int32_t ch, float fontSize);
float ApplySelectorShape(SelectorShape shape, float t);
float CombineSelectorValues(SelectorMode mode, float a, float b);
float ComputeRangeSelectorFactor(const RangeSelector* selector, size_t glyphIndex,
                                 size_t totalGlyphs);

struct ArcLengthLUT {
  std::vector<float> arcLengths = {};
  std::vector<Point> positions = {};
  std::vector<float> tangents = {};
  float totalLength = 0.0f;
};

ArcLengthLUT BuildArcLengthLUT(const PathData& pathData, int samplesPerSegment = 16);
void SampleArcLengthLUT(const ArcLengthLUT& lut, float arcLength, Point* outPos, float* outTangent,
                        bool closed = false);

//==============================================================================
// HTMLWriterContext
//==============================================================================

class HTMLWriterContext {
 public:
  float docWidth = 0;
  float docHeight = 0;
  std::unordered_set<const Composition*> visitedCompositions = {};

  // Guard against runaway recursion through writeLayer / writeGroup / writeComposition /
  // writeElements. 512 frames keeps well below an 8 MB default thread stack even if each frame
  // consumes a few KB. Exceeding this limit causes the current subtree to be silently skipped.
  int recursionDepth = 0;
  static constexpr int MAX_RECURSION_DEPTH = 512;

  // Static-image rasterization config, forwarded from HTMLExportOptions. Empty `staticImgDir`
  // disables rasterization entirely; the writer then emits nothing for Diamond/tiled-pattern
  // fills (they are CSS-inexpressible without the PNG).
  std::string staticImgDir = {};
  std::string staticImgUrlPrefix = "static-img/";
  std::string staticImgNamePrefix = {};
  float staticImgPixelRatio = 2.0f;

  // Font synthesis control, forwarded from HTMLExportOptions.
  bool fontSynthesisWeight = true;
  bool fontSynthesisStyle = true;

  // Per-plusDarker-Layer backdrop data registered by HTMLPlusDarkerRenderer before the writer
  // walks the tree. When a Layer pointer is present here, the writer emits an SVG filter that
  // composites the backdrop via feImage + feComposite arithmetic instead of the mix-blend-mode
  // approximation.
  std::unordered_map<const Layer*, PlusDarkerBackdrop> plusDarkerBackdrops = {};

  // Set by writeLayer before calling writeRepeater, when the layer div was shifted into
  // negative quadrants to cover every Repeater copy's union bounds. Each copy's transform
  // must be prepended with `translate(-repeaterOriginOffset)` so its in-content origin still
  // maps to the layer origin in world space. Reset to (0, 0) when no shift was applied.
  float repeaterOriginOffsetX = 0;
  float repeaterOriginOffsetY = 0;

  // Set by writeLayer at the same time as repeaterOriginOffset, but NOT cleared by writeRepeater.
  // Child layers (layer->children) inside a Repeater-expanded parent div need to add this offset
  // to their own renderPos so they stay at the correct document position despite the parent div
  // having been shifted up/left by the Repeater union-bounds expansion.
  // Consumed (read + cleared) by each child writeLayer call, then re-set for the next sibling
  // by the parent writeLayerInner child loop before each writeLayer invocation.
  float childLayerOffsetX = 0;
  float childLayerOffsetY = 0;

  // Saved copy of childLayerOffset for the current layer's children, set immediately after the
  // Repeater offset is computed. The child loop uses this to restore childLayerOffset before each
  // sibling writeLayer call (childLayerOffset is consumed/cleared on each invocation).
  float savedChildLayerOffsetX = 0;
  float savedChildLayerOffsetY = 0;

  // Set by writeLayer before emitting the layer's contents. When non-empty, the layer has hoisted
  // font CSS onto its own style attribute, so child Text spans should skip the font-* properties
  // that are already covered by this signature. Reset to empty when leaving the layer.
  FontSignature fontHoistSignature = {};

  // Cache: path d-string → assigned ID in global <defs>. Used to deduplicate repeated SVG paths
  // (e.g. from Repeater nodes) by emitting <path id="p0" d="..."/> once and referencing via <use>.
  std::unordered_map<std::string, std::string> pathDefIds = {};

  // Cache: filter signature → assigned ID in global <defs>. Used to deduplicate repeated
  // <filter> definitions (e.g. multiple layers with identical DropShadowStyle or LayerFilter
  // chain). Signature is a string concatenation of filter parameters, prefixed with the
  // emission-site tag ("chain:", "dss:", "iss:", "issb:") so different sites never collide.
  std::unordered_map<std::string, std::string> filterDefIds = {};

  std::string nextId(const std::string& prefix) {
    return prefix + std::to_string(_id++);
  }

 private:
  int _id = 0;
};

class RecursionGuard {
 public:
  explicit RecursionGuard(HTMLWriterContext* c) : _ctx(c) {
    _overflowed = _ctx->recursionDepth >= HTMLWriterContext::MAX_RECURSION_DEPTH;
    if (!_overflowed) {
      _ctx->recursionDepth++;
    }
  }
  ~RecursionGuard() {
    if (!_overflowed) {
      _ctx->recursionDepth--;
    }
  }
  RecursionGuard(const RecursionGuard&) = delete;
  RecursionGuard& operator=(const RecursionGuard&) = delete;

  bool overflowed() const {
    return _overflowed;
  }

 private:
  HTMLWriterContext* _ctx = nullptr;
  bool _overflowed = false;
};

//==============================================================================
// HTMLWriter
//==============================================================================

class HTMLWriter {
 public:
  HTMLWriter(HTMLBuilder* defs, HTMLWriterContext* ctx) : _defs(defs), _ctx(ctx) {
  }

  // `parentLayout` is the flex layout mode of the layer's immediate PAGX parent. It is used to
  // decide which axis is the main axis when deciding whether `flex="N"` should emit a CSS `flex`
  // declaration: PAGX disables flex growth on a child that declares an explicit main-axis size,
  // but CSS `flex: N` (shorthand for `flex: N 1 0%`) resets basis to 0 and would grow the child
  // regardless. Pass LayoutMode::None for root children or composition-owned layers that have no
  // PAGX parent flex container.
  void writeLayer(HTMLBuilder& out, const Layer* layer, float parentAlpha = 1.0f,
                  bool distributeAlpha = false, bool isFlexItem = false,
                  LayoutMode parentLayout = LayoutMode::None);

 private:
  HTMLBuilder* _defs = nullptr;
  HTMLWriterContext* _ctx = nullptr;

  struct GeoInfo {
    NodeType type = NodeType::Rectangle;
    const Element* element = nullptr;
    std::string modifiedPathData = {};
    // Padding of the nearest layout container (Layer / Group) that owns this element. Needed by
    // renderCSSDiv to decide whether the `inset:0` shortcut is safe: when the container has any
    // padding, a Rectangle with left=right=top=bottom=0 must shrink inside that padded content
    // box, which `inset:0` cannot express (CSS inset is relative to the parent's border box,
    // not its padding box). In that case we fall back to the absolute-pixel branch whose
    // left/top/width/height already reflect PAGX's padding-inset layout resolution.
    const Padding* parentPadding = nullptr;
  };

  // Maps a PAGX Stroke's `align` attribute to a CSS `-webkit-text-stroke` width plus optional
  // `paint-order: stroke fill`, reproducing tgfx's Fill-then-Stroke layering across all text
  // emit paths (writeText, writeTextModifier, writeTextPath, TextBox rich-text / single-span).
  // Shared so TextBox span paths produce the same StrokeAlign mapping as plain Text paths;
  // otherwise a Stroke authored with align="Center" inside a TextBox would render with the
  // wider Outside geometry simply because it went through a different dispatcher.
  struct TextStrokeCss {
    float width = 0.0f;
    bool paintOrderStrokeFill = false;
  };
  static TextStrokeCss ResolveTextStrokeCss(float width, StrokeAlign align, bool hasFill);

  // For vertical-writing-mode TextBoxes, inserts a literal '\n' (which the HTML emit path
  // turns into `<br>`) before every glyph that tgfx placed into a new column. Without this,
  // Chromium re-runs its own line-break algorithm on the raw text and — lacking PAGX's UAX-14
  // punctuation-squash rules — picks different column boundaries (e.g. for `「你好」。「世界」`
  // in a 100x160 vertical box, tgfx packs the leading paren and closing punctuation onto
  // column 1 and the second `「世界」` onto column 2, while Chromium splits `「世界」` across
  // two columns).
  //
  // Column transitions are detected by `|x_i - x_{i-1}| >= renderFontSize/2`. The threshold is
  // intentionally large because Latin/digit glyphs in a CJK vertical column sit ~1.54px off
  // the CJK baseline X (HarfBuzz vertical metrics for Noto Sans SC), and that small offset
  // must NOT trigger a column break or every CJK<->Latin handoff inside a single column would
  // be split (broke vertical.pagx's `2024年12月31日` and Latin/CJK mixed samples in an earlier
  // attempt).
  //
  // Source `\n` characters are passed through untouched and suppress the very next auto-break
  // so we don't emit two consecutive `<br>`s when the author already requested a column break
  // that coincides with a tgfx column transition. If the source visible-codepoint count
  // differs from the laid-out glyph count (ligature/cluster collapse) we return the input
  // unchanged so the caller falls back to plain text. Returns the source text unchanged when
  // no rewriting is needed (single column, non-vertical text, or null input) so callers can
  // chain unconditionally. Lives on HTMLWriter so it can access Text's private `glyphData`
  // through the existing `friend class HTMLWriter` declaration without adding a new friend.
  static std::string rewriteVerticalColumnBreaks(const Text* text);

  // Color source conversions. `boxLeft`/`boxTop`/`boxWidth`/`boxHeight` describe the element
  // box (in the gradient source's coordinate space) that the CSS background will paint into.
  // They are used by linear-gradient to emulate PAGX's startPoint/endPoint semantics since
  // CSS linear-gradient's geometry is defined relative to the element's own box rather than
  // absolute coordinates. Pass zero/NaN sizes when the box is irrelevant (e.g. SVG fills).
  std::string colorToCSS(const ColorSource* src, float* outAlpha, float boxLeft = 0,
                         float boxTop = 0, float boxWidth = 0, float boxHeight = 0);
  std::string colorToSVGFill(const ColorSource* src, float* outAlpha, float bboxX = 0,
                             float bboxY = 0, float bboxW = 0, float bboxH = 0);
  void writeSVGGradientDef(const ColorSource* src, const std::string& id, float bboxX = 0,
                           float bboxY = 0, float bboxW = 0, float bboxH = 0);
  // Like writeSVGGradientDef but writes into an arbitrary HTMLBuilder instead of _defs.
  // Use this when the gradient must live in the same SVG as the geometry that references it
  // (required for userSpaceOnUse coordinates to be resolved in the correct coordinate system).
  void writeSVGGradientDefInto(HTMLBuilder& builder, const ColorSource* src, const std::string& id,
                               float bboxX = 0, float bboxY = 0, float bboxW = 0, float bboxH = 0);

  // Rendering
  void writeLayerContents(HTMLBuilder& out, const Layer* layer, float alpha, bool distribute,
                          LayerPlacement targetPlacement);
  // Emits only the layer's inner children (background contents, composition, child layers,
  // foreground contents) without opening an outer layer <div>. Extracted so the BlurFilter
  // mirror-tile emission path can replay the same inner DOM inside each of the 9 mirrored tiles.
  void writeLayerInner(HTMLBuilder& out, const Layer* layer, float contentAlpha,
                       bool childDistribute, bool isFlexContainer);
  // `containerPadding` is the padding of the nearest layout container (Layer / Group) supplying
  // these elements. It propagates into each emitted GeoInfo so that stretch-style Rectangles
  // (left=right=top=bottom=0) can detect a padded parent and avoid the `inset:0` shortcut that
  // would otherwise paint into the full border box instead of the padded content box.
  void writeElements(HTMLBuilder& out, const std::vector<Element*>& elements, float alpha,
                     bool distribute, LayerPlacement targetPlacement,
                     const Padding* containerPadding = nullptr);
  // Appends an SVG <filter> definition that composites the layer's pre-rendered backdrop with its
  // own pixels using feComposite arithmetic (k1=0, k2=1, k3=1, k4=-1), yielding PlusDarker
  // semantics: clamp(Sc + Dc - 1, 0, 1). Called once per plusDarker layer at the point the
  // layer's <div> is emitted.
  void emitPlusDarkerFilterDef(const PlusDarkerBackdrop& backdrop);
  void renderGeo(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const Fill* fill,
                 const Stroke* stroke, float alpha, bool hasTrim, const TrimPath* trim,
                 bool hasMerge, MergePathMode mergeMode = MergePathMode::Append);
  bool canCSS(const std::vector<GeoInfo>& geos, const Fill* fill, const Stroke* stroke,
              bool hasTrim, bool hasMerge);
  void renderCSSDiv(HTMLBuilder& out, const GeoInfo& geo, const Fill* fill, float alpha,
                    BlendMode painterBlend);
  void renderDiamondCanvas(HTMLBuilder& out, const GeoInfo& geo, const Fill* fill, float alpha,
                           BlendMode painterBlend);
  void renderConicCanvas(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const Fill* fill,
                         float alpha, BlendMode painterBlend);
  void renderImagePatternCanvas(HTMLBuilder& out, const GeoInfo& geo, const Fill* fill, float alpha,
                                BlendMode painterBlend);
  void renderSVG(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const Fill* fill,
                 const Stroke* stroke, float alpha, BlendMode painterBlend,
                 const TrimPath* trim = nullptr, MergePathMode mergeMode = MergePathMode::Append);
  void writeText(HTMLBuilder& out, const Text* text, const Fill* fill, const Stroke* stroke,
                 const TextBox* tb, float alpha);
  void writeTextModifier(HTMLBuilder& out, const std::vector<GeoInfo>& geos,
                         const TextModifier* modifier, const Fill* fill, const Stroke* stroke,
                         const TextBox* tb, float alpha);
  void writeTextPath(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const TextPath* textPath,
                     const Fill* fill, const Stroke* stroke, const TextBox* tb, float alpha);
  // Renders a Text node whose glyphRuns hold embedded vector/bitmap shapes (not real text).
  // Only call when text->text is empty or text->fontFamily is empty. Real text must go through
  // the CSS span path in writeText to preserve gradient fills and layout semantics.
  void writeEmbeddedShapeGlyphs(HTMLBuilder& out, const Text* text, const Fill* fill,
                                const Stroke* stroke, float alpha);
  // `parentMatrix` is the accumulated transform of any enclosing Groups that were flattened
  // into the current element stream (writeElements inlines flattened-Group geometry via
  // TransformPathDataToSVG but emits nested Groups by recursing into writeGroup). For Groups
  // reached from Layer contents or Composition roots the caller passes identity; when a flattened
  // outer Group encounters an inner Group it must pass the outer Group's matrix so the inner
  // Group's CSS transform stacks on top, since the inner Group's renderPosition is expressed in
  // the outer Group's local space (not the enclosing Layer's).
  void writeGroup(HTMLBuilder& out, const Group* group, float alpha, bool distribute,
                  const Matrix& parentMatrix = {});
  void writeRepeater(HTMLBuilder& out, const Repeater* rep, const std::vector<GeoInfo>& geos,
                     const Fill* fill, const Stroke* stroke, float alpha,
                     const TrimPath* trim = nullptr, bool applyCopyAlphaDecay = true);
  void writeComposition(HTMLBuilder& out, const Composition* comp, float alpha = 1.0f,
                        bool distribute = false);
  void paintGeos(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const Fill* fill,
                 const Stroke* stroke, const TextBox* textBox, float alpha, bool hasTrim,
                 const TrimPath* curTrim, bool hasMerge, MergePathMode mergeMode);
  void applyTrimAttrs(HTMLBuilder& builder, const TrimPath* trim);
  void applyTrimAttrsContinuous(HTMLBuilder& builder, const TrimPath* trim,
                                const std::vector<float>& pathLengths, float totalLength,
                                size_t geoIndex);
  bool isContinuousTrimVisible(const TrimPath* trim, const std::vector<float>& pathLengths,
                               float totalLength, size_t geoIndex);
  float computeGeoPathLength(const GeoInfo& geo);

  // Filter defs
  std::string writeFilterDefs(const std::vector<LayerFilter*>& filters);

  // Returns true when the layer is a candidate for the 3x3 mirror-tile DOM emission path used
  // to approximate BlurFilter.tileMode=Mirror in HTML. The path requires a single BlurFilter
  // with tileMode=Mirror and no LayerStyles (drop shadows / inner shadows / backdrop blurs) on
  // the same layer, because those would interact non-trivially with the duplicated subtree.
  static bool needsMirrorTiling(const Layer* layer);

  // Mask/clip defs
  std::string writeMaskCSS(const Layer* mask, MaskType type, Point maskedLayerPos = {});
  std::string writeClipDef(const Layer* mask);
  void writeClipContent(HTMLBuilder& out, const Layer* layer, const Matrix& parent);

  // SVG fill/stroke attributes
  void applySVGFill(HTMLBuilder& out, const Fill* fill, float bboxX = 0, float bboxY = 0,
                    float bboxW = 0, float bboxH = 0);
  void applySVGStroke(HTMLBuilder& out, const Stroke* stroke, float pathLength = 0.0f);

  // Returns the ID for a path definition in the global <defs>. If the d-string was already
  // defined, returns the existing ID. Otherwise, emits a bare <path id="id" d="d"/> (no
  // fill/stroke) into _defs and caches the mapping. The caller applies fill/stroke on the
  // <use> element that references this definition.
  std::string getOrCreatePathDef(const std::string& d);

  // Filter dedup helpers. Signatures are built by each emission site (writeFilterDefs,
  // DropShadowStyle, InnerShadowStyle variants) from the filter parameters and a site-specific
  // prefix. On cache hit, callers skip emission and reuse the cached id.
  std::string lookupFilterId(const std::string& signature) const;
  void registerFilterId(const std::string& signature, const std::string& id);

  // Computes the axis-aligned bounding box of a geo list expanded by `pad` on every side. When
  // `useModifiedPathData` is true, entries with non-empty `modifiedPathData` use the bounds of
  // that modified path; otherwise the entry always falls through to the 4-type switch over the
  // element's own render position and size (Rectangle / Ellipse / Path / Polystar). Returns
  // true with outX / outY / outW / outH set when the resulting box has positive area; returns
  // false when no geo contributed usable bounds (callers should skip emission).
  static bool ComputeGeosBoundingBox(const std::vector<GeoInfo>& geos, float pad,
                                     bool useModifiedPathData, float& outX, float& outY,
                                     float& outW, float& outH);
};

}  // namespace pagx
