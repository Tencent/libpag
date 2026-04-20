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
#include <unordered_set>
#include <vector>
#include "pagx/html/HTMLBuilder.h"
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
std::string Matrix3DToCSS(const Matrix3D& m);
const char* BlendModeToMixBlendMode(BlendMode mode);
Color LerpColor(const Color& a, const Color& b, float t);
std::string LayerTransformCSS(const Layer* layer);
Matrix BuildGroupMatrix(const Group* group);
const char* AlignmentToCSS(Alignment alignment);
const char* ArrangementToCSS(Arrangement arrangement);
std::string PaddingToCSS(const Padding& padding);

std::string GetImageSrc(const Image* image);
const char* DetectImageMime(const uint8_t* bytes, size_t size);

// Mirrors tgfx/src/core/utils/FauxBoldScale.cpp. Returns the stroke width (in px) that produces
// the same visible thickening as tgfx's faux-bold path rendering at the given fontSize.
float FauxBoldStrokeWidth(float fontSize);
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

struct DiamondGradientInfo {
  std::string canvasId;
  float unitMatrix[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  std::vector<std::pair<float, Color>> stops;
  float width = 0;
  float height = 0;
};

struct ImagePatternCanvasInfo {
  std::string canvasId;
  std::string imageSrc;
  int wrapS = 0x2901;
  int wrapT = 0x2901;
  int filter = 0x2601;
  float width = 0;
  float height = 0;
};

//==============================================================================
// HTMLWriterContext
//==============================================================================

class HTMLWriterContext {
 public:
  float docWidth = 0;
  float docHeight = 0;
  std::unordered_set<const Composition*> visitedCompositions = {};
  std::vector<DiamondGradientInfo> diamondGradients = {};
  std::vector<ImagePatternCanvasInfo> imagePatternCanvases = {};

  std::string nextId(const std::string& prefix) {
    return prefix + std::to_string(_id++);
  }

 private:
  int _id = 0;
};

//==============================================================================
// HTMLWriter
//==============================================================================

class HTMLWriter {
 public:
  HTMLWriter(HTMLBuilder* defs, HTMLWriterContext* ctx) : _defs(defs), _ctx(ctx) {
  }

  void writeLayer(HTMLBuilder& out, const Layer* layer, float parentAlpha = 1.0f,
                  bool distributeAlpha = false, bool isFlexItem = false);

 private:
  HTMLBuilder* _defs = nullptr;
  HTMLWriterContext* _ctx = nullptr;

  struct GeoInfo {
    NodeType type = NodeType::Rectangle;
    const Element* element = nullptr;
    std::string modifiedPathData = {};
  };

  // Color source conversions. `boxLeft`/`boxTop`/`boxWidth`/`boxHeight` describe the element
  // box (in the gradient source's coordinate space) that the CSS background will paint into.
  // They are used by linear-gradient to emulate PAGX's startPoint/endPoint semantics since
  // CSS linear-gradient's geometry is defined relative to the element's own box rather than
  // absolute coordinates. Pass zero/NaN sizes when the box is irrelevant (e.g. SVG fills).
  std::string colorToCSS(const ColorSource* src, float* outAlpha, float boxLeft = 0,
                         float boxTop = 0, float boxWidth = 0, float boxHeight = 0);
  std::string colorToSVGFill(const ColorSource* src, float* outAlpha);
  void writeSVGGradientDef(const ColorSource* src, const std::string& id);

  // Rendering
  void writeLayerContents(HTMLBuilder& out, const Layer* layer, float alpha, bool distribute,
                          LayerPlacement targetPlacement);
  void writeElements(HTMLBuilder& out, const std::vector<Element*>& elements, float alpha,
                     bool distribute, LayerPlacement targetPlacement);
  void renderGeo(HTMLBuilder& out, const std::vector<GeoInfo>& geos, const Fill* fill,
                 const Stroke* stroke, float alpha, bool hasTrim, const TrimPath* trim,
                 bool hasMerge, MergePathMode mergeMode = MergePathMode::Append);
  bool canCSS(const std::vector<GeoInfo>& geos, const Fill* fill, const Stroke* stroke,
              bool hasTrim, bool hasMerge);
  void renderCSSDiv(HTMLBuilder& out, const GeoInfo& geo, const Fill* fill, float alpha,
                    BlendMode painterBlend);
  void renderDiamondCanvas(HTMLBuilder& out, const GeoInfo& geo, const Fill* fill, float alpha,
                           BlendMode painterBlend);
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
  void writeGlyphRunSVG(HTMLBuilder& out, const Text* text, const Fill* fill, const Stroke* stroke,
                        float alpha);
  void writeGroup(HTMLBuilder& out, const Group* group, float alpha, bool distribute);
  void writeRepeater(HTMLBuilder& out, const Repeater* rep, const std::vector<GeoInfo>& geos,
                     const Fill* fill, const Stroke* stroke, float alpha,
                     const TrimPath* trim = nullptr);
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

  // Mask/clip defs
  std::string writeMaskCSS(const Layer* mask, MaskType type);
  std::string writeClipDef(const Layer* mask);
  void writeClipContent(HTMLBuilder& out, const Layer* layer, const Matrix& parent);

  // SVG fill/stroke attributes
  void applySVGFill(HTMLBuilder& out, const Fill* fill);
  void applySVGStroke(HTMLBuilder& out, const Stroke* stroke, float pathLength = 0.0f);
};

}  // namespace pagx
