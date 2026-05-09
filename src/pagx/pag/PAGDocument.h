// Copyright (C) 2026 Tencent. All rights reserved.
//
// PAGDocument data model — authoritative definitions are in design doc
// appendix C (sections C.1 through C.9). Codec.cpp / Baker.cpp / Inflater.cpp
// all consume this header; nothing else should declare these types.
//
// Memory ownership: the document tree is built up of std::unique_ptr children
// so destruction is recursive without manual code. Top-level handle is
// std::unique_ptr<PAGDocument> per §8.3bis (RAII discipline).
//
// Property<T> default-value rule: every Property field below must spell out
// its defaultValue inside the `= MakeProp(X)` initializer (and ONLY there) so
// the Codec's Read/Write can pass the same X to WriteProperty / ReadProperty
// without re-hardcoding it (§C.2 nuance).
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include "pag/file.h"   // ::pag::Ratio
#include "pag/types.h"  // ::pag::ParagraphJustification / ::pag::TextDirection
#include "pagx/pag/PropertyEncoding.h"
#include "tgfx/core/BlendMode.h"
#include "tgfx/core/Color.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Image.h"  // ImageAsset::decodedImage cache
#include "tgfx/core/Matrix.h"
#include "tgfx/core/Matrix3D.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/PathTypes.h"
#include "tgfx/core/Point.h"
#include "tgfx/core/RSXform.h"
#include "tgfx/core/Rect.h"
#include "tgfx/core/SamplingOptions.h"
#include "tgfx/core/TileMode.h"
#include "tgfx/layers/LayerMaskType.h"
#include "tgfx/layers/LayerPaint.h"  // LayerPlacement
#include "tgfx/layers/StrokeAlign.h"
#include "tgfx/layers/vectors/FillStyle.h"     // FillRule
#include "tgfx/layers/vectors/MergePath.h"     // MergePathOp
#include "tgfx/layers/vectors/Polystar.h"      // PolystarType
#include "tgfx/layers/vectors/Repeater.h"      // RepeaterOrder
#include "tgfx/layers/vectors/TextSelector.h"  // SelectorUnit / SelectorShape / SelectorMode
#include "tgfx/layers/vectors/TrimPath.h"      // TrimPathType

namespace pagx::pag {

// ============================================================================
// C.1 — version / type aliases
// ============================================================================

constexpr uint8_t FORMAT_VERSION = 0x02;
constexpr uint8_t COMPRESSION_UNCOMPRESSED = 0x00;

// PAGDocument speaks tgfx semantics directly — Baker has already done the
// PAGX→tgfx enum translation (P3→sRGB, blend mode mapping, ...). Aliasing
// keeps Codec / Inflater code free of tgfx:: prefixes when the type is
// already canonical to PAG v2.
using BlendMode = tgfx::BlendMode;
using LayerMaskType = tgfx::LayerMaskType;
using LineCap = tgfx::LineCap;
using LineJoin = tgfx::LineJoin;
using StrokeAlign = tgfx::StrokeAlign;
using LayerPlacement = tgfx::LayerPlacement;
using TileMode = tgfx::TileMode;
using FilterMode = tgfx::FilterMode;
using MipmapMode = tgfx::MipmapMode;
using FillRule = tgfx::FillRule;
using PolystarType = tgfx::PolystarType;
using MergePathOp = tgfx::MergePathOp;
using TrimPathType = tgfx::TrimPathType;
using RepeaterOrder = tgfx::RepeaterOrder;
using SelectorUnit = tgfx::SelectorUnit;
using SelectorShape = tgfx::SelectorShape;
using SelectorMode = tgfx::SelectorMode;
using SamplingOptions = tgfx::SamplingOptions;
using Color = tgfx::Color;
using Point = tgfx::Point;
using Rect = tgfx::Rect;
using Matrix = tgfx::Matrix;
using Matrix3D = tgfx::Matrix3D;
using Path = tgfx::Path;
using Ratio = ::pag::Ratio;

// Text layout enums reused from libpag v1 TextDocument (Phase 16 runtime-shape
// mode mirrors v1 TextDocument field set).
using ParagraphJustification = ::pag::ParagraphJustification;
using TextDirection = ::pag::TextDirection;

// ImagePattern fill scaling (no direct tgfx equivalent; Inflater branches by value).
enum class ScaleMode : uint8_t { None = 0, Stretch = 1, LetterBox = 2, Zoom = 3 };

// Text alignment (tgfx has no first-class enum; renderer's TextLayer dispatches by value).
enum class TextAlign : uint8_t { Start = 0, Center = 1, End = 2, Justify = 3 };

// ============================================================================
// C.3 — Layer / VectorElement / ColorSource / Filter / Style discriminators
// ============================================================================

enum class LayerType : uint8_t {
  Layer = 0,
  Shape = 1,
  Text = 2,
  Image = 3,
  Solid = 4,
  Vector = 5,
  Mesh = 6,  // not produced this cycle; reserved
  CompositionRef = 7,
};

enum class VectorElementType : uint8_t {
  Rectangle = 0,
  Ellipse = 1,
  Polystar = 2,
  ShapePath = 3,
  FillStyle = 4,
  StrokeStyle = 5,
  TrimPath = 6,
  RoundCorner = 7,
  MergePath = 8,
  Repeater = 9,
  Text = 10,
  TextPath = 11,
  TextModifier = 12,
  VectorGroup = 13,
};

enum class ColorSourceType : uint8_t {
  SolidColor = 0,
  LinearGradient = 1,
  RadialGradient = 2,
  ConicGradient = 3,
  DiamondGradient = 4,
  ImagePattern = 5,
};

enum class LayerFilterType : uint8_t {
  Blur = 0,
  DropShadow = 1,
  InnerShadow = 2,
  ColorMatrix = 3,
  Blend = 4,
};

enum class LayerStyleType : uint8_t {
  DropShadow = 0,
  InnerShadow = 1,
  BackgroundBlur = 2,
};

enum class ImageAssetKind : uint8_t {
  Raster = 0,  // only kind produced this cycle
  Svg = 1,     // reserved; Decoder warns + falls back to Raster
  Video = 2,   // reserved
  Hdr = 3,     // reserved
};

// ============================================================================
// C.4 — ImageAsset (Phase 16: font resources no longer embedded; see §10)
// ============================================================================

struct ImageAsset {
  // Raw encoded bytes (PNG / JPEG / WebP). shared_ptr enables zero-copy
  // through the loader (Phase 10.5 will wire ZeroCopyScope). Reset to null
  // by the Inflater after the first successful decode (§11.1
  // `ImageBytesReleasedAfterInflate` contract).
  std::shared_ptr<const tgfx::Data> data;
  // Cached decoded image, populated by the Inflater on first decode and
  // reused on subsequent calls. Lets a single ImageAsset back many
  // ImagePattern color sources without re-decoding or hitting the
  // post-reset sentinel branch (image_pattern.pagx exercises 4 layers
  // sharing one logo, regression originally fixed during Phase 17 wrap-up).
  // tgfx::Image is itself shared_ptr-managed, so reuse keeps GPU/CPU
  // decode caches single-instanced.
  std::shared_ptr<tgfx::Image> decodedImage;
  int32_t width = 0;
  int32_t height = 0;
  ImageAssetKind kind = ImageAssetKind::Raster;
};

// ============================================================================
// C.4b — EmbeddedFont (Phase 17 v2.23: path-based font resource)
// ============================================================================
//
// Mirrors PAGX <Font id="fontN"><Glyph advance="..." path="..."/></Font>.
// Only consumed by case A (author-authored <GlyphRun>) via
// ElementTextData::glyphRuns[i].embeddedFontIndex. NOT a ttf subset:
// ttf / otf font files are never embedded in PAG.
//
// Coordinate convention: glyph paths retain the PAGX y-up source (e.g.
// "M100 0 L193 -299" where negative y is above); the Inflater flips to
// tgfx's y-down convention via Matrix::Scale(scale, -scale) at render time.

struct EmbeddedGlyph {
  float advance = 0.0f;  // PAGX <Glyph advance="..."> — em-units, pre-unitsPerEm
  tgfx::Path path;       // PAGX <Glyph path="..."> — y-up source coordinates
};

struct EmbeddedFont {
  uint32_t unitsPerEm = 1000;         // PAGX default
  std::vector<EmbeddedGlyph> glyphs;  // index = value stored in GlyphRunData.glyphs
};

// ============================================================================
// C.6 — ShapeStyleData (color source for Fill / Stroke)
// ============================================================================

struct ShapeStyleData {
  ColorSourceType sourceType = ColorSourceType::SolidColor;

  // Common
  Property<float> alpha = MakeProp(1.0f);
  BlendMode blendMode = BlendMode::SrcOver;  // setter only triggers in Inflater when non-SrcOver
  bool overrideBlendMode = false;

  // SolidColor
  Property<Color> color = MakeProp(Color{});

  // Gradient common (flattened pagx::Gradient base, include/pagx/nodes/Gradient.h)
  Property<std::vector<Color>> stopColors = MakeProp<std::vector<Color>>({});
  Property<std::vector<float>> stopPositions = MakeProp<std::vector<float>>({});
  Property<Matrix> gradientMatrix = MakeProp(Matrix::I());
  bool fitsToGeometry = true;

  // Linear
  Property<Point> startPoint = MakeProp(Point{0.0f, 0.0f});
  Property<Point> endPoint = MakeProp(Point{1.0f, 0.0f});

  // Radial / Conic / Diamond
  Property<Point> center = MakeProp(Point{});
  Property<float> radius = MakeProp(0.0f);
  Property<float> startAngle = MakeProp(0.0f);
  Property<float> endAngle = MakeProp(360.0f);

  // ImagePattern
  uint32_t patternImageIndex = UINT32_MAX;  // sentinel: image missing
  TileMode tileModeX = TileMode::Decal;
  TileMode tileModeY = TileMode::Decal;
  FilterMode filterMode = FilterMode::Linear;
  MipmapMode mipmapMode = MipmapMode::None;
  ScaleMode scaleMode = ScaleMode::LetterBox;
  Property<Matrix> patternMatrix = MakeProp(Matrix::I());
};

// ============================================================================
// C.7 — VectorElement variant + per-element payloads
// ============================================================================

// Forward declarations for the variant alternatives below.
struct ElementRectangleData;
struct ElementEllipseData;
struct ElementPolystarData;
struct ElementShapePathData;
struct ElementFillStyleData;
struct ElementStrokeStyleData;
struct ElementTrimPathData;
struct ElementRoundCornerData;
struct ElementMergePathData;
struct ElementRepeaterData;
struct ElementTextData;
struct ElementTextPathData;
struct ElementTextModifierData;
struct ElementVectorGroupData;
struct RangeSelectorData;

// One std::variant beats 14 unique_ptr fields per VectorElement (saves
// ~112 bytes/element). Discipline (§C.7):
//   - VectorElement::type must match the alternative held in `payload`.
//   - Read side reads `type` first, then emplaces the alternative.
//   - Write side dispatches via std::visit.
using VectorElementPayload =
    std::variant<std::monostate, std::unique_ptr<ElementRectangleData>,
                 std::unique_ptr<ElementEllipseData>, std::unique_ptr<ElementPolystarData>,
                 std::unique_ptr<ElementShapePathData>, std::unique_ptr<ElementFillStyleData>,
                 std::unique_ptr<ElementStrokeStyleData>, std::unique_ptr<ElementTrimPathData>,
                 std::unique_ptr<ElementRoundCornerData>, std::unique_ptr<ElementMergePathData>,
                 std::unique_ptr<ElementRepeaterData>, std::unique_ptr<ElementTextData>,
                 std::unique_ptr<ElementTextPathData>, std::unique_ptr<ElementTextModifierData>,
                 std::unique_ptr<ElementVectorGroupData>>;

struct VectorElement {
  VectorElementType type = VectorElementType::Rectangle;
  VectorElementPayload payload = {};
};

struct ElementRectangleData {
  Property<Point> position = MakeProp(Point{});
  Property<Point> size = MakeProp(Point{});
  Property<float> roundness = MakeProp(0.0f);
  bool reversed = false;
};

struct ElementEllipseData {
  Property<Point> position = MakeProp(Point{});
  Property<Point> size = MakeProp(Point{});
  bool reversed = false;
};

struct ElementPolystarData {
  Property<Point> position = MakeProp(Point{});
  Property<float> pointCount = MakeProp(5.0f);
  Property<float> outerRadius = MakeProp(0.0f);
  Property<float> innerRadius = MakeProp(0.0f);
  Property<float> outerRoundness = MakeProp(0.0f);
  Property<float> innerRoundness = MakeProp(0.0f);
  Property<float> rotation = MakeProp(0.0f);
  bool reversed = false;
  PolystarType polystarType = PolystarType::Polygon;
};

struct ElementShapePathData {
  Property<Point> position = MakeProp(Point{});
  Property<Path> path = MakeProp(Path{});
  bool reversed = false;
};

struct ElementFillStyleData {
  std::unique_ptr<ShapeStyleData> style;
  FillRule fillRule = FillRule::Winding;
  LayerPlacement placement = LayerPlacement::Background;
};

struct ElementStrokeStyleData {
  std::unique_ptr<ShapeStyleData> style;
  Property<float> strokeWidth = MakeProp(1.0f);
  LineCap lineCap = LineCap::Butt;
  LineJoin lineJoin = LineJoin::Miter;
  Property<float> miterLimit = MakeProp(4.0f);
  Property<std::vector<float>> lineDashPattern = MakeProp<std::vector<float>>({});
  Property<float> lineDashPhase = MakeProp(0.0f);
  bool lineDashAdaptive = false;
  StrokeAlign strokeAlign = StrokeAlign::Center;
  LayerPlacement placement = LayerPlacement::Background;
};

struct ElementTrimPathData {
  Property<float> start = MakeProp(0.0f);
  Property<float> end = MakeProp(100.0f);
  Property<float> offset = MakeProp(0.0f);
  // Note: design doc §C.7 spells the default as "Simultaneously" but tgfx's
  // TrimPathType enum only declares Separate / Continuous (Separate carries
  // the equivalent "trim each path with the same parameters" semantics).
  TrimPathType trimType = TrimPathType::Separate;
};

struct ElementRoundCornerData {
  Property<float> radius = MakeProp(0.0f);
};

struct ElementMergePathData {
  MergePathOp mode = MergePathOp::Append;
};

struct ElementRepeaterData {
  Property<float> copies = MakeProp(1.0f);
  Property<float> offset = MakeProp(0.0f);
  RepeaterOrder order = RepeaterOrder::BelowOriginal;
  Property<Point> anchor = MakeProp(Point{});
  Property<Point> position = MakeProp(Point{});
  Property<float> rotation = MakeProp(0.0f);
  Property<Point> scale = MakeProp(Point{1.0f, 1.0f});
  Property<float> startAlpha = MakeProp(1.0f);
  Property<float> endAlpha = MakeProp(1.0f);
};

struct ElementTextData {
  // Phase 16 (v2.20): runtime-shape mode. Field set mirrors libpag v1
  // pag::TextDocument (include/pag/types.h:class PAG_API TextDocument);
  // GlyphRunBlob has been dropped. Inflater calls TextShaper::Shape + the
  // v1 paragraph layout at load time.
  Property<Point> position = MakeProp(Point{});
  // PAGX-exclusive per-glyph anchors; preserved so Inflater can pass them
  // to tgfx::Text::Make(blob, anchors).
  Property<std::vector<Point>> anchors = MakeProp<std::vector<Point>>({});

  // ----- Content (shaper input) -----
  std::string text = "";
  std::string fontFamily = "";
  std::string fontStyle = "";
  float fontSize = 12.0f;

  // ----- Direction / paragraph layout (aligns with v1 TextDocument) -----
  TextDirection direction = TextDirection::Default;
  ParagraphJustification justification = ParagraphJustification::LeftJustify;
  float leading = 0.0f;   // Line leading; 0 = auto (fontSize * 1.2).
  float tracking = 0.0f;  // Letter spacing.
  float firstBaseLine = 0.0f;
  float baselineShift = 0.0f;

  // ----- BoxText -----
  bool boxText = false;
  Point boxTextPos = {};
  Point boxTextSize = {};

  // ----- Faux style (synthesized when no matching bold/italic typeface) ----
  bool fauxBold = false;
  bool fauxItalic = false;

  // ----- Paint -----
  bool applyFill = true;
  bool applyStroke = false;
  bool strokeOverFill = true;
  Color fillColor = {};
  Color strokeColor = {};
  float strokeWidth = 1.0f;

  // ----- Background (aligns with v1 TextSourceV2) -----
  Color backgroundColor = {};
  uint8_t backgroundAlpha = 0;

  // ===== Phase 17 v2.23: case A / case B authoritative storage =====
  //
  // Exactly one of `glyphRuns` (case A) and `shapedGlyphs` (case B) is
  // non-empty per ElementText, mirroring the PAGX-side branch the Baker
  // observed. Both empty means an empty Text (no UTF-8 content and no
  // author runs) — the Inflater drops it silently.

  // Case A: PAGX Text has author <GlyphRun> referencing an embedded <Font>.
  // Snapshot of those runs is stored verbatim here; the referenced path-
  // based font resource is Bake-time interned into PAGDocument.embeddedFonts.
  // The Inflater rebuilds a tgfx::PathTypefaceBuilder-backed typeface and
  // replays the glyph stream — no FontProvider involvement, exact author
  // geometry preserved.
  struct GlyphRunData {
    uint32_t embeddedFontIndex = 0;  // → PAGDocument.embeddedFonts[i]
    float fontSize = 12.0f;
    std::vector<tgfx::GlyphID> glyphs;   // indexes into EmbeddedFont.glyphs
    float x = 0.0f;                      // overall X offset (PAGX GlyphRun.x)
    float y = 0.0f;                      // overall Y offset
    std::vector<float> xOffsets;         // optional per-glyph X
    std::vector<tgfx::Point> positions;  // optional per-glyph (x,y)
    // Author-layer per-glyph static xforms (PAGX `<GlyphRun anchors="..."
    // scales="..." rotations="..." skews="...">`). PAGX→SVG already applies
    // these (src/pagx/svg/SVGTextLayout.cpp:313-342); PAG side is dormant —
    // current spec/samples don't populate them, so Baker writes nothing and
    // Codec only round-trips empty vectors. Activate Baker/Inflater when a
    // real sample appears in CrossCheck FAIL list. Distinct from TextModifier
    // animation, which is a separate vector element wired via ElementTextModifier.
    std::vector<tgfx::Point> anchors;
    std::vector<tgfx::Point> scales;
    std::vector<float> rotations;
    std::vector<float> skews;
  };
  std::vector<GlyphRunData> glyphRuns = {};

  // Case B: PAGX Text is pure <Text text="..."> without <GlyphRun>. PAGX
  // TextLayout in applyLayout() has already resolved glyph IDs + positions
  // per run; Baker snapshots them verbatim. Inflater uses FontProvider to
  // resolve typeface and assembles tgfx::TextBlob directly — no re-shape,
  // no re-layout. positions are layout-absolute (PathA-equivalent); the
  // Inflater applies pay.position via setPosition exactly the way PAGX-
  // native LayerBuilder::convertText does with renderPosition().
  struct ShapedGlyphRun {
    std::string typefaceFamily;
    std::string typefaceStyle;
    std::string typefaceKey;  // family|style|unitsPerEm|glyphsCount
    float fontSize = 12.0f;
    std::vector<tgfx::GlyphID> glyphs;
    std::vector<tgfx::Point> positions;
    std::vector<tgfx::RSXform> xforms;  // empty unless vertical writing mode
  };
  std::vector<ShapedGlyphRun> shapedGlyphs = {};
};

struct ElementTextPathData {
  Property<Path> path = MakeProp(Path{});
  Property<Point> baselineOrigin = MakeProp(Point{});
  Property<float> baselineAngle = MakeProp(0.0f);
  Property<float> firstMargin = MakeProp(0.0f);
  Property<float> lastMargin = MakeProp(0.0f);
  bool perpendicular = false;
  bool reversed = false;
  bool forceAlignment = false;
};

struct RangeSelectorData {
  Property<float> start = MakeProp(0.0f);
  Property<float> end = MakeProp(100.0f);
  Property<float> offset = MakeProp(0.0f);
  SelectorUnit unit = SelectorUnit::Percentage;
  SelectorShape shape = SelectorShape::Square;
  Property<float> easeIn = MakeProp(0.0f);
  Property<float> easeOut = MakeProp(0.0f);
  SelectorMode mode = SelectorMode::Add;
  Property<float> weight = MakeProp(100.0f);
  bool randomOrder = false;
  uint16_t randomSeed = 0;
};

struct ElementTextModifierData {
  // Transform
  Property<Point> anchor = MakeProp(Point{});
  Property<Point> position = MakeProp(Point{});
  Property<float> rotation = MakeProp(0.0f);
  Property<Point> scale = MakeProp(Point{1.0f, 1.0f});
  Property<float> skew = MakeProp(0.0f);
  Property<float> skewAxis = MakeProp(0.0f);
  Property<float> alpha = MakeProp(1.0f);

  // Optional paint overrides (has* gates the corresponding Property)
  bool hasFillColor = false;
  Property<Color> fillColor = MakeProp(Color{});
  bool hasStrokeColor = false;
  Property<Color> strokeColor = MakeProp(Color{});
  bool hasStrokeWidth = false;
  Property<float> strokeWidth = MakeProp(0.0f);

  std::vector<std::unique_ptr<RangeSelectorData>> rangeSelectors = {};
};

struct ElementVectorGroupData {
  std::vector<std::unique_ptr<VectorElement>> elements = {};
  Property<Point> anchor = MakeProp(Point{});
  Property<Point> position = MakeProp(Point{});
  Property<Point> scale = MakeProp(Point{1.0f, 1.0f});
  Property<float> rotation = MakeProp(0.0f);
  Property<float> alpha = MakeProp(1.0f);
  Property<float> skew = MakeProp(0.0f);
  Property<float> skewAxis = MakeProp(0.0f);
};

// ============================================================================
// C.8 — Layer payload alternatives
// ============================================================================

struct ShapePayload {
  // Reserved for tgfx::ShapeLayer integration — PAGX itself routes through VectorLayer
  // instead this cycle, so Baker leaves ShapePayload empty.
  Property<Path> path = MakeProp(Path{});
  std::vector<std::unique_ptr<ShapeStyleData>> fillStyles = {};
  std::vector<std::unique_ptr<ShapeStyleData>> strokeStyles = {};
  Property<float> strokeWidth = MakeProp(1.0f);
  LineCap lineCap = LineCap::Butt;
  LineJoin lineJoin = LineJoin::Miter;
  Property<float> miterLimit = MakeProp(4.0f);
  Property<std::vector<float>> lineDashPattern = MakeProp<std::vector<float>>({});
  Property<float> lineDashPhase = MakeProp(0.0f);
  bool lineDashAdaptive = false;
  StrokeAlign strokeAlign = StrokeAlign::Center;
  bool strokeOnTop = false;
};

struct TextPayload {
  // Reserved; PAGX does not currently route rich text through LayerType::Text
  // (everything goes through VectorLayer + ElementText). Phase 16 (v2.20)
  // drops fontIndex along with PAGDocument::fonts[] and instead carries
  // family/style strings directly, matching runtime-shape mode.
  Property<std::string> text = MakeProp<std::string>({});
  Property<Color> textColor = MakeProp(Color{});
  std::string fontFamily = {};
  std::string fontStyle = {};
  Property<float> fontSize = MakeProp(12.0f);
  Property<float> width = MakeProp(0.0f);
  Property<float> height = MakeProp(0.0f);
  TextAlign textAlign = TextAlign::Start;
  bool autoWrap = false;
};

struct ImagePayload {
  uint32_t imageIndex = UINT32_MAX;
  SamplingOptions sampling = {};
};

struct SolidPayload {
  Property<float> width = MakeProp(0.0f);
  Property<float> height = MakeProp(0.0f);
  Property<float> radiusX = MakeProp(0.0f);
  Property<float> radiusY = MakeProp(0.0f);
  Property<Color> color = MakeProp(Color{});
};

struct VectorPayload {
  std::vector<std::unique_ptr<VectorElement>> contents = {};
};

struct MeshPayload {
  // Empty this cycle. Codec Read/Write returns a default-constructed object
  // without consuming or producing any bytes.
};

// ============================================================================
// C.9 — LayerFilter / LayerStyle
// ============================================================================

struct LayerFilter {
  LayerFilterType type = LayerFilterType::Blur;

  // Blur / InnerShadow common
  Property<float> blurX = MakeProp(0.0f);
  Property<float> blurY = MakeProp(0.0f);
  TileMode tileMode = TileMode::Decal;

  // DropShadow / InnerShadow common
  Property<float> offsetX = MakeProp(0.0f);
  Property<float> offsetY = MakeProp(0.0f);
  Property<Color> color = MakeProp(Color{});
  bool shadowOnly = false;

  // Blend
  Property<Color> blendColor = MakeProp(Color{});
  BlendMode blendFilterMode = BlendMode::SrcOver;

  // ColorMatrix
  Property<std::array<float, 20>> colorMatrix = MakeProp(std::array<float, 20>{});
};

struct LayerStyle {
  LayerStyleType type = LayerStyleType::DropShadow;
  BlendMode blendMode = BlendMode::SrcOver;
  bool excludeChildEffects = false;

  // DropShadow / InnerShadow / BackgroundBlur common
  Property<float> blurX = MakeProp(0.0f);
  Property<float> blurY = MakeProp(0.0f);

  // DropShadow / InnerShadow common
  Property<float> offsetX = MakeProp(0.0f);
  Property<float> offsetY = MakeProp(0.0f);
  Property<Color> color = MakeProp(Color{});
  bool showBehindLayer = false;  // DropShadow only

  // BackgroundBlur
  TileMode tileMode = TileMode::Decal;
};

// ============================================================================
// C.5 — Composition / Layer
// ============================================================================

struct Composition;
struct Layer;

struct Layer {
  LayerType type = LayerType::Layer;
  std::string name = {};

  // Timeline (static this cycle).
  uint32_t startTime = 0;
  uint32_t duration = 1;
  Ratio stretch = {1, 1};

  // Animatable core.
  Property<bool> visible = MakeProp(true);
  Property<float> alpha = MakeProp(1.0f);
  Property<BlendMode> blendMode = MakeProp(BlendMode::SrcOver);
  Property<Matrix> matrix = MakeProp(Matrix::I());
  Property<Matrix3D> matrix3D = MakeProp(Matrix3D::I());
  Property<Rect> scrollRect = MakeProp(Rect{});

  // Non-animatable flags.
  bool hasScrollRect = false;
  bool preserve3D = false;
  bool passThroughBackground = true;
  bool allowsEdgeAntialiasing = true;
  bool allowsGroupOpacity = true;

  // Mask: chain of child indices from the enclosing root to the mask target.
  // Empty vector means the layer has no mask binding.
  std::vector<uint32_t> maskLayerPath = {};
  LayerMaskType maskType = LayerMaskType::Alpha;

  // Effects.
  std::vector<std::unique_ptr<LayerFilter>> filters = {};
  std::vector<std::unique_ptr<LayerStyle>> styles = {};

  // Hierarchy.
  std::vector<std::unique_ptr<Layer>> children = {};

  // Subtype payload — exactly the one matching `type` should be populated.
  std::unique_ptr<ShapePayload> shape;
  std::unique_ptr<TextPayload> text;
  std::unique_ptr<ImagePayload> image;
  std::unique_ptr<SolidPayload> solid;
  std::unique_ptr<VectorPayload> vector;
  std::unique_ptr<MeshPayload> mesh;
  uint32_t compositionIndex = UINT32_MAX;  // type == CompositionRef
};

struct Composition {
  std::string id = {};
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<std::unique_ptr<Layer>> layers = {};
};

// ============================================================================
// C.5-pre — FileHeader / PAGDocument (top-level root)
// ============================================================================

struct FileHeader {
  float width = 0.0f;
  float height = 0.0f;
  Color backgroundColor = Color{};
  Ratio frameRate = {24, 1};
  uint32_t duration = 1;  // frames
};

struct PAGDocument {
  FileHeader header = {};
  std::vector<std::unique_ptr<ImageAsset>> images = {};
  // Phase 17 v2.23: path-based embedded fonts referenced by ElementTextData
  // case A glyphRuns. NOT ttf/otf font files; see EmbeddedFont definition.
  // Top-level Tag order: ImageAssetTable → EmbeddedFontTable → CompositionList.
  std::vector<std::unique_ptr<EmbeddedFont>> embeddedFonts = {};
  std::vector<std::unique_ptr<Composition>> compositions = {};
};

}  // namespace pagx::pag
