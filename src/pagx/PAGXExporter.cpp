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

#include "pagx/PAGXExporter.h"
#include <cmath>
#include <cstdio>
#include "pagx/PAGXDefaults.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorMatrixFilter.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowFilter.h"
#include "pagx/nodes/InnerShadowStyle.h"
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
#include "pagx/svg/SVGPathParser.h"
#include "pagx/utils/Base64.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

//==============================================================================
// XMLBuilder - XML generation helper
//==============================================================================

class XMLBuilder {
 public:
  XMLBuilder() {
    tagStack.reserve(32);
  }

  void appendDeclaration() {
    buffer += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  }

  void openElement(const char* tag) {
    writeIndent();
    buffer += "<";
    buffer += tag;
    tagStack.push_back(tag);
  }

  void addAttribute(const char* name, const std::string& value) {
    if (!value.empty()) {
      buffer += " ";
      buffer += name;
      buffer += "=\"";
      buffer += escapeXML(value);
      buffer += "\"";
    }
  }

  void addAttribute(const char* name, float value, float defaultValue = 0) {
    if (value != defaultValue) {
      buffer += " ";
      buffer += name;
      buffer += "=\"";
      buffer += FloatToString(value);
      buffer += "\"";
    }
  }

  void addRequiredAttribute(const char* name, float value) {
    buffer += " ";
    buffer += name;
    buffer += "=\"";
    buffer += FloatToString(value);
    buffer += "\"";
  }

  void addRequiredAttribute(const char* name, const std::string& value) {
    buffer += " ";
    buffer += name;
    buffer += "=\"";
    buffer += escapeXML(value);
    buffer += "\"";
  }

  void addAttribute(const char* name, int value, int defaultValue = 0) {
    if (value != defaultValue) {
      buffer += " ";
      buffer += name;
      buffer += "=\"";
      buffer += std::to_string(value);
      buffer += "\"";
    }
  }

  void addAttribute(const char* name, bool value, bool defaultValue = false) {
    if (value != defaultValue) {
      buffer += " ";
      buffer += name;
      buffer += "=\"";
      buffer += (value ? "true" : "false");
      buffer += "\"";
    }
  }

  void addOptionalAttribute(const char* name, float value) {
    if (!std::isnan(value)) {
      buffer += " ";
      buffer += name;
      buffer += "=\"";
      buffer += FloatToString(value);
      buffer += "\"";
    }
  }

  void closeElementStart() {
    buffer += ">\n";
    indentLevel++;
  }

  void closeElementSelfClosing() {
    buffer += "/>\n";
    tagStack.pop_back();
  }

  void closeElement() {
    indentLevel--;
    writeIndent();
    buffer += "</";
    buffer += tagStack.back();
    buffer += ">\n";
    tagStack.pop_back();
  }

  void writeRaw(const std::string& content) {
    buffer += content;
  }

  void writeRawLine(const std::string& content) {
    // Handle multi-line content by indenting each line individually.
    size_t start = 0;
    while (start < content.size()) {
      auto end = content.find('\n', start);
      if (end == std::string::npos) {
        writeIndent();
        buffer.append(content, start, content.size() - start);
        buffer += "\n";
        break;
      }
      writeIndent();
      buffer.append(content, start, end - start);
      buffer += "\n";
      start = end + 1;
    }
  }

  void writeComment(const std::string& text) {
    writeIndent();
    buffer += "<!-- ";
    buffer += text;
    buffer += " -->\n";
  }

  std::string release() {
    return std::move(buffer);
  }

 private:
  std::string buffer = {};
  std::vector<const char*> tagStack = {};
  int indentLevel = 0;

  void writeIndent() {
    buffer.append(static_cast<size_t>(indentLevel * 2), ' ');
  }

  static std::string escapeXML(const std::string& input) {
    size_t extraSize = 0;
    for (char c : input) {
      switch (c) {
        case '&':
          extraSize += 4;  // &amp;
          break;
        case '<':
          extraSize += 3;  // &lt;
          break;
        case '"':
          extraSize += 5;  // &quot;
          break;
        case '\'':
          extraSize += 5;  // &apos;
          break;
        case '\n':
          extraSize += 4;  // &#10;
          break;
        case '\r':
          extraSize += 4;  // &#13;
          break;
        case '\t':
          extraSize += 3;  // &#9;
          break;
        default:
          break;
      }
    }
    if (extraSize == 0) {
      return input;
    }
    std::string result;
    result.reserve(input.size() + extraSize);
    for (char c : input) {
      switch (c) {
        case '&':
          result += "&amp;";
          break;
        case '<':
          result += "&lt;";
          break;
        case '"':
          result += "&quot;";
          break;
        case '\'':
          result += "&apos;";
          break;
        case '\n':
          result += "&#10;";
          break;
        case '\r':
          result += "&#13;";
          break;
        case '\t':
          result += "&#9;";
          break;
        default:
          result += c;
          break;
      }
    }
    return result;
  }
};

//==============================================================================
// Helper functions for converting types to strings
//==============================================================================

static std::string PointToString(const Point& p) {
  return FloatToString(p.x) + "," + FloatToString(p.y);
}

static std::string SizeToString(const Size& s) {
  return FloatToString(s.width) + "," + FloatToString(s.height);
}

static std::string RectToString(const Rect& r) {
  return FloatToString(r.x) + "," + FloatToString(r.y) + "," + FloatToString(r.width) + "," +
         FloatToString(r.height);
}

static std::string FloatListToString(const float* values, size_t count) {
  std::string result;
  result.reserve(count * 8);
  for (size_t i = 0; i < count; i++) {
    if (i > 0) {
      result += ",";
    }
    result += FloatToString(values[i]);
  }
  return result;
}

static std::string FloatListToString(const std::vector<float>& values) {
  return FloatListToString(values.data(), values.size());
}

static std::string PointListToString(const std::vector<Point>& points) {
  std::string result = {};
  result.reserve(points.size() * 12);
  for (size_t i = 0; i < points.size(); i++) {
    if (i > 0) {
      result += ";";
    }
    result += FloatToString(points[i].x) + "," + FloatToString(points[i].y);
  }
  return result;
}

static bool ShouldSkipPosition(const Point& position, const Point& defaultPos, float left,
                               float top, float right, float bottom, float centerX, float centerY) {
  bool hasH = !std::isnan(left) || !std::isnan(right) || !std::isnan(centerX);
  bool hasV = !std::isnan(top) || !std::isnan(bottom) || !std::isnan(centerY);
  bool isDefault = (position.x == defaultPos.x && position.y == defaultPos.y);
  return (hasH && hasV) || isDefault;
}

//==============================================================================
// Forward declarations
//==============================================================================

using Options = PAGXExporter::Options;

static void WriteColorSource(XMLBuilder& xml, const ColorSource* node);
static void WriteVectorElement(XMLBuilder& xml, const Element* node, const Options& options);
static void WriteLayerStyle(XMLBuilder& xml, const LayerStyle* node);
static void WriteLayerFilter(XMLBuilder& xml, const LayerFilter* node);
static void WriteResource(XMLBuilder& xml, const Node* node, const Options& options);
static void WriteLayer(XMLBuilder& xml, const Layer* node, const Options& options);

static void WriteCustomData(XMLBuilder& xml, const Node* node) {
  for (const auto& [key, value] : node->customData) {
    if (Node::IsValidCustomDataKey(key)) {
      xml.addAttribute(("data-" + key).c_str(), value);
    }
  }
}

//==============================================================================
// ColorStop and ColorSource writing
//==============================================================================

static bool WriteColorAttribute(XMLBuilder& xml, const ColorSource* color) {
  if (!color) {
    return false;
  }
  if (!color->id.empty()) {
    xml.addAttribute("color", "@" + color->id);
    return false;
  }
  if (color->nodeType() == NodeType::SolidColor) {
    auto solid = static_cast<const SolidColor*>(color);
    xml.addAttribute("color", ColorToHexString(solid->color, solid->color.alpha < 1.0f));
    return false;
  }
  return true;
}

static void WriteColorStops(XMLBuilder& xml, const std::vector<ColorStop*>& stops) {
  for (const auto* stop : stops) {
    xml.openElement("ColorStop");
    xml.addRequiredAttribute("offset", stop->offset);
    xml.addRequiredAttribute("color", ColorToHexString(stop->color, stop->color.alpha < 1.0f));
    WriteCustomData(xml, stop);
    xml.closeElementSelfClosing();
  }
}

static void WriteGradientMatrixAndStops(XMLBuilder& xml, const Matrix& matrix,
                                        const std::vector<ColorStop*>& colorStops) {
  if (!matrix.isIdentity()) {
    xml.addAttribute("matrix", MatrixToString(matrix));
  }
  if (colorStops.empty()) {
    xml.closeElementSelfClosing();
  } else {
    xml.closeElementStart();
    WriteColorStops(xml, colorStops);
    xml.closeElement();
  }
}

static void WriteColorSource(XMLBuilder& xml, const ColorSource* node) {
  switch (node->nodeType()) {
    case NodeType::SolidColor: {
      auto solid = static_cast<const SolidColor*>(node);
      xml.openElement("SolidColor");
      xml.addAttribute("id", solid->id);
      xml.addAttribute("color", ColorToHexString(solid->color, solid->color.alpha < 1.0f));
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::LinearGradient: {
      auto grad = static_cast<const LinearGradient*>(node);
      xml.openElement("LinearGradient");
      xml.addAttribute("id", grad->id);
      if (grad->startPoint != Default<LinearGradient>().startPoint) {
        xml.addAttribute("startPoint", PointToString(grad->startPoint));
      }
      xml.addRequiredAttribute("endPoint", PointToString(grad->endPoint));
      WriteCustomData(xml, node);
      WriteGradientMatrixAndStops(xml, grad->matrix, grad->colorStops);
      break;
    }
    case NodeType::RadialGradient: {
      auto grad = static_cast<const RadialGradient*>(node);
      xml.openElement("RadialGradient");
      xml.addAttribute("id", grad->id);
      if (grad->center != Default<RadialGradient>().center) {
        xml.addAttribute("center", PointToString(grad->center));
      }
      xml.addRequiredAttribute("radius", grad->radius);
      WriteCustomData(xml, node);
      WriteGradientMatrixAndStops(xml, grad->matrix, grad->colorStops);
      break;
    }
    case NodeType::ConicGradient: {
      auto grad = static_cast<const ConicGradient*>(node);
      xml.openElement("ConicGradient");
      xml.addAttribute("id", grad->id);
      if (grad->center != Default<ConicGradient>().center) {
        xml.addAttribute("center", PointToString(grad->center));
      }
      xml.addAttribute("startAngle", grad->startAngle, Default<ConicGradient>().startAngle);
      xml.addAttribute("endAngle", grad->endAngle, Default<ConicGradient>().endAngle);
      WriteCustomData(xml, node);
      WriteGradientMatrixAndStops(xml, grad->matrix, grad->colorStops);
      break;
    }
    case NodeType::DiamondGradient: {
      auto grad = static_cast<const DiamondGradient*>(node);
      xml.openElement("DiamondGradient");
      xml.addAttribute("id", grad->id);
      if (grad->center != Default<DiamondGradient>().center) {
        xml.addAttribute("center", PointToString(grad->center));
      }
      xml.addRequiredAttribute("radius", grad->radius);
      WriteCustomData(xml, node);
      WriteGradientMatrixAndStops(xml, grad->matrix, grad->colorStops);
      break;
    }
    case NodeType::ImagePattern: {
      auto pattern = static_cast<const ImagePattern*>(node);
      xml.openElement("ImagePattern");
      xml.addAttribute("id", pattern->id);
      if (pattern->image != nullptr) {
        if (!pattern->image->id.empty()) {
          xml.addAttribute("image", "@" + pattern->image->id);
        } else if (pattern->image->data) {
          xml.addAttribute("image",
                           "data:image/png;base64," + Base64Encode(pattern->image->data->bytes(),
                                                                   pattern->image->data->size()));
        } else if (!pattern->image->filePath.empty()) {
          xml.addAttribute("image", pattern->image->filePath);
        }
      }
      if (pattern->tileModeX != Default<ImagePattern>().tileModeX) {
        xml.addAttribute("tileModeX", TileModeToString(pattern->tileModeX));
      }
      if (pattern->tileModeY != Default<ImagePattern>().tileModeY) {
        xml.addAttribute("tileModeY", TileModeToString(pattern->tileModeY));
      }
      if (pattern->filterMode != Default<ImagePattern>().filterMode) {
        xml.addAttribute("filterMode", FilterModeToString(pattern->filterMode));
      }
      if (pattern->mipmapMode != Default<ImagePattern>().mipmapMode) {
        xml.addAttribute("mipmapMode", MipmapModeToString(pattern->mipmapMode));
      }
      if (!pattern->matrix.isIdentity()) {
        xml.addAttribute("matrix", MatrixToString(pattern->matrix));
      }
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    default:
      break;
  }
}

//==============================================================================
// VectorElement writing
//==============================================================================

static void WriteVectorElement(XMLBuilder& xml, const Element* node, const Options& options) {
  switch (node->nodeType()) {
    case NodeType::Rectangle: {
      auto rect = static_cast<const Rectangle*>(node);
      xml.openElement("Rectangle");
      Point rectDefaultPos = {rect->size.width * 0.5f, rect->size.height * 0.5f};
      if (!ShouldSkipPosition(rect->position, rectDefaultPos, rect->left, rect->top, rect->right,
                              rect->bottom, rect->centerX, rect->centerY)) {
        xml.addAttribute("position", PointToString(rect->position));
      }
      if (rect->size.width != 0 || rect->size.height != 0) {
        xml.addAttribute("size", SizeToString(rect->size));
      }
      xml.addAttribute("roundness", rect->roundness, Default<Rectangle>().roundness);
      xml.addAttribute("reversed", rect->reversed, Default<Rectangle>().reversed);
      xml.addOptionalAttribute("left", rect->left);
      xml.addOptionalAttribute("right", rect->right);
      xml.addOptionalAttribute("top", rect->top);
      xml.addOptionalAttribute("bottom", rect->bottom);
      xml.addOptionalAttribute("centerX", rect->centerX);
      xml.addOptionalAttribute("centerY", rect->centerY);
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::Ellipse: {
      auto ellipse = static_cast<const Ellipse*>(node);
      xml.openElement("Ellipse");
      Point ellipseDefaultPos = {ellipse->size.width * 0.5f, ellipse->size.height * 0.5f};
      if (!ShouldSkipPosition(ellipse->position, ellipseDefaultPos, ellipse->left, ellipse->top,
                              ellipse->right, ellipse->bottom, ellipse->centerX,
                              ellipse->centerY)) {
        xml.addAttribute("position", PointToString(ellipse->position));
      }
      if (ellipse->size.width != 0 || ellipse->size.height != 0) {
        xml.addAttribute("size", SizeToString(ellipse->size));
      }
      xml.addAttribute("reversed", ellipse->reversed, Default<Ellipse>().reversed);
      xml.addOptionalAttribute("left", ellipse->left);
      xml.addOptionalAttribute("right", ellipse->right);
      xml.addOptionalAttribute("top", ellipse->top);
      xml.addOptionalAttribute("bottom", ellipse->bottom);
      xml.addOptionalAttribute("centerX", ellipse->centerX);
      xml.addOptionalAttribute("centerY", ellipse->centerY);
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::Polystar: {
      auto polystar = static_cast<const Polystar*>(node);
      xml.openElement("Polystar");
      auto polyBounds = polystar->getContentBounds();
      Point polyDefaultPos = {-polyBounds.x, -polyBounds.y};
      if (!ShouldSkipPosition(polystar->position, polyDefaultPos, polystar->left, polystar->top,
                              polystar->right, polystar->bottom, polystar->centerX,
                              polystar->centerY)) {
        xml.addAttribute("position", PointToString(polystar->position));
      }
      xml.addAttribute("type", PolystarTypeToString(polystar->type));
      xml.addAttribute("pointCount", polystar->pointCount, Default<Polystar>().pointCount);
      xml.addAttribute("outerRadius", polystar->outerRadius, Default<Polystar>().outerRadius);
      xml.addAttribute("innerRadius", polystar->innerRadius, Default<Polystar>().innerRadius);
      xml.addAttribute("rotation", polystar->rotation, Default<Polystar>().rotation);
      xml.addAttribute("outerRoundness", polystar->outerRoundness,
                       Default<Polystar>().outerRoundness);
      xml.addAttribute("innerRoundness", polystar->innerRoundness,
                       Default<Polystar>().innerRoundness);
      xml.addAttribute("reversed", polystar->reversed, Default<Polystar>().reversed);
      xml.addOptionalAttribute("left", polystar->left);
      xml.addOptionalAttribute("right", polystar->right);
      xml.addOptionalAttribute("top", polystar->top);
      xml.addOptionalAttribute("bottom", polystar->bottom);
      xml.addOptionalAttribute("centerX", polystar->centerX);
      xml.addOptionalAttribute("centerY", polystar->centerY);
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::Path: {
      auto path = static_cast<const Path*>(node);
      xml.openElement("Path");
      if (path->data != nullptr && !path->data->id.empty()) {
        // Use the reference to PathData resource.
        xml.addAttribute("data", "@" + path->data->id);
      } else if (path->data != nullptr && !path->data->isEmpty()) {
        // Inline the path data.
        xml.addAttribute("data", PathDataToSVGString(*path->data));
      }
      xml.addAttribute("reversed", path->reversed, Default<Path>().reversed);
      if (!ShouldSkipPosition(path->position, {0, 0}, path->left, path->top, path->right,
                              path->bottom, path->centerX, path->centerY)) {
        xml.addAttribute("position", PointToString(path->position));
      }
      xml.addOptionalAttribute("left", path->left);
      xml.addOptionalAttribute("right", path->right);
      xml.addOptionalAttribute("top", path->top);
      xml.addOptionalAttribute("bottom", path->bottom);
      xml.addOptionalAttribute("centerX", path->centerX);
      xml.addOptionalAttribute("centerY", path->centerY);
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::Text: {
      auto text = static_cast<const Text*>(node);
      xml.openElement("Text");
      if (!text->text.empty()) {
        xml.addAttribute("text", text->text);
      }
      if (!ShouldSkipPosition(text->position, {0, 0}, text->left, text->top, text->right,
                              text->bottom, text->centerX, text->centerY)) {
        xml.addAttribute("position", PointToString(text->position));
      }
      xml.addAttribute("fontFamily", text->fontFamily);
      if (!text->fontStyle.empty()) {
        xml.addAttribute("fontStyle", text->fontStyle);
      }
      xml.addAttribute("fontSize", text->fontSize, Default<Text>().fontSize);
      xml.addAttribute("letterSpacing", text->letterSpacing, Default<Text>().letterSpacing);
      xml.addAttribute("fauxBold", text->fauxBold, Default<Text>().fauxBold);
      xml.addAttribute("fauxItalic", text->fauxItalic, Default<Text>().fauxItalic);
      if (text->textAnchor != Default<Text>().textAnchor) {
        xml.addAttribute("textAnchor", TextAnchorToString(text->textAnchor));
      }
      if (text->baseline != Default<Text>().baseline) {
        xml.addAttribute("baseline", TextBaselineToString(text->baseline));
      }
      xml.addOptionalAttribute("left", text->left);
      xml.addOptionalAttribute("right", text->right);
      xml.addOptionalAttribute("top", text->top);
      xml.addOptionalAttribute("bottom", text->bottom);
      xml.addOptionalAttribute("centerX", text->centerX);
      xml.addOptionalAttribute("centerY", text->centerY);
      WriteCustomData(xml, node);
      if (options.skipGlyphData || text->glyphRuns.empty()) {
        xml.closeElementSelfClosing();
      } else {
        xml.closeElementStart();
        for (const auto& run : text->glyphRuns) {
          xml.openElement("GlyphRun");
          if (run->font != nullptr && !run->font->id.empty()) {
            xml.addAttribute("font", "@" + run->font->id);
          }
          xml.addAttribute("fontSize", run->fontSize, Default<GlyphRun>().fontSize);

          // Write glyphs as comma-separated integers
          if (!run->glyphs.empty()) {
            std::string glyphsStr = {};
            glyphsStr.reserve(run->glyphs.size() * 6);
            for (size_t i = 0; i < run->glyphs.size(); i++) {
              if (i > 0) {
                glyphsStr += ",";
              }
              glyphsStr += std::to_string(run->glyphs[i]);
            }
            xml.addRequiredAttribute("glyphs", glyphsStr);
          }

          // Write x/y overall offsets (only if non-zero)
          xml.addAttribute("x", run->x, Default<GlyphRun>().x);
          xml.addAttribute("y", run->y, Default<GlyphRun>().y);

          // Write xOffsets (comma-separated)
          if (!run->xOffsets.empty()) {
            xml.addRequiredAttribute("xOffsets", FloatListToString(run->xOffsets));
          }

          // Write positions (semicolon-separated x,y pairs)
          if (!run->positions.empty()) {
            xml.addRequiredAttribute("positions", PointListToString(run->positions));
          }

          // Write anchors (semicolon-separated x,y pairs)
          if (!run->anchors.empty()) {
            xml.addRequiredAttribute("anchors", PointListToString(run->anchors));
          }

          // Write scales (semicolon-separated sx,sy pairs)
          if (!run->scales.empty()) {
            xml.addRequiredAttribute("scales", PointListToString(run->scales));
          }

          // Write rotations (comma-separated angles in degrees)
          if (!run->rotations.empty()) {
            xml.addRequiredAttribute("rotations", FloatListToString(run->rotations));
          }

          // Write skews (comma-separated angles in degrees)
          if (!run->skews.empty()) {
            xml.addRequiredAttribute("skews", FloatListToString(run->skews));
          }

          // Write linebox bounds (x,y,w,h)
          if (run->bounds.width > 0 || run->bounds.height > 0) {
            float boundsValues[] = {run->bounds.x, run->bounds.y, run->bounds.width,
                                    run->bounds.height};
            xml.addRequiredAttribute("bounds", FloatListToString(boundsValues, 4));
          }

          WriteCustomData(xml, run);
          xml.closeElementSelfClosing();
        }
        xml.closeElement();
      }
      break;
    }
    case NodeType::Fill: {
      auto fill = static_cast<const Fill*>(node);
      xml.openElement("Fill");
      bool needsInlineColorSource = WriteColorAttribute(xml, fill->color);
      xml.addAttribute("alpha", fill->alpha, Default<Fill>().alpha);
      if (fill->blendMode != Default<Fill>().blendMode) {
        xml.addAttribute("blendMode", BlendModeToString(fill->blendMode));
      }
      if (fill->fillRule != Default<Fill>().fillRule) {
        xml.addAttribute("fillRule", FillRuleToString(fill->fillRule));
      }
      if (fill->placement != Default<Fill>().placement) {
        xml.addAttribute("placement", LayerPlacementToString(fill->placement));
      }
      WriteCustomData(xml, node);
      if (needsInlineColorSource) {
        xml.closeElementStart();
        WriteColorSource(xml, fill->color);
        xml.closeElement();
      } else {
        xml.closeElementSelfClosing();
      }
      break;
    }
    case NodeType::Stroke: {
      auto stroke = static_cast<const Stroke*>(node);
      xml.openElement("Stroke");
      bool needsInlineColorSource = WriteColorAttribute(xml, stroke->color);
      xml.addAttribute("width", stroke->width, Default<Stroke>().width);
      xml.addAttribute("alpha", stroke->alpha, Default<Stroke>().alpha);
      if (stroke->blendMode != Default<Stroke>().blendMode) {
        xml.addAttribute("blendMode", BlendModeToString(stroke->blendMode));
      }
      if (stroke->cap != Default<Stroke>().cap) {
        xml.addAttribute("cap", LineCapToString(stroke->cap));
      }
      if (stroke->join != Default<Stroke>().join) {
        xml.addAttribute("join", LineJoinToString(stroke->join));
      }
      xml.addAttribute("miterLimit", stroke->miterLimit, Default<Stroke>().miterLimit);
      if (!stroke->dashes.empty()) {
        xml.addAttribute("dashes", FloatListToString(stroke->dashes));
      }
      xml.addAttribute("dashOffset", stroke->dashOffset, Default<Stroke>().dashOffset);
      xml.addAttribute("dashAdaptive", stroke->dashAdaptive, Default<Stroke>().dashAdaptive);
      if (stroke->align != Default<Stroke>().align) {
        xml.addAttribute("align", StrokeAlignToString(stroke->align));
      }
      if (stroke->placement != Default<Stroke>().placement) {
        xml.addAttribute("placement", LayerPlacementToString(stroke->placement));
      }
      WriteCustomData(xml, node);
      if (needsInlineColorSource) {
        xml.closeElementStart();
        WriteColorSource(xml, stroke->color);
        xml.closeElement();
      } else {
        xml.closeElementSelfClosing();
      }
      break;
    }
    case NodeType::TrimPath: {
      auto trim = static_cast<const TrimPath*>(node);
      xml.openElement("TrimPath");
      xml.addAttribute("start", trim->start, Default<TrimPath>().start);
      xml.addAttribute("end", trim->end, Default<TrimPath>().end);
      xml.addAttribute("offset", trim->offset, Default<TrimPath>().offset);
      if (trim->type != Default<TrimPath>().type) {
        xml.addAttribute("type", TrimTypeToString(trim->type));
      }
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::RoundCorner: {
      auto round = static_cast<const RoundCorner*>(node);
      xml.openElement("RoundCorner");
      xml.addAttribute("radius", round->radius, Default<RoundCorner>().radius);
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::MergePath: {
      auto merge = static_cast<const MergePath*>(node);
      xml.openElement("MergePath");
      if (merge->mode != Default<MergePath>().mode) {
        xml.addAttribute("mode", MergePathModeToString(merge->mode));
      }
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::TextModifier: {
      auto modifier = static_cast<const TextModifier*>(node);
      xml.openElement("TextModifier");
      if (modifier->anchor != Default<TextModifier>().anchor) {
        xml.addAttribute("anchor", PointToString(modifier->anchor));
      }
      if (modifier->position != Default<TextModifier>().position) {
        xml.addAttribute("position", PointToString(modifier->position));
      }
      xml.addAttribute("rotation", modifier->rotation, Default<TextModifier>().rotation);
      if (modifier->scale != Default<TextModifier>().scale) {
        xml.addAttribute("scale", PointToString(modifier->scale));
      }
      xml.addAttribute("skew", modifier->skew, Default<TextModifier>().skew);
      xml.addAttribute("skewAxis", modifier->skewAxis, Default<TextModifier>().skewAxis);
      xml.addAttribute("alpha", modifier->alpha, Default<TextModifier>().alpha);
      if (modifier->fillColor.has_value()) {
        xml.addAttribute("fillColor", ColorToHexString(modifier->fillColor.value()));
      }
      if (modifier->strokeColor.has_value()) {
        xml.addAttribute("strokeColor", ColorToHexString(modifier->strokeColor.value()));
      }
      if (modifier->strokeWidth.has_value()) {
        xml.addAttribute("strokeWidth", modifier->strokeWidth.value());
      }
      WriteCustomData(xml, node);
      if (modifier->selectors.empty()) {
        xml.closeElementSelfClosing();
      } else {
        xml.closeElementStart();
        for (const auto& selector : modifier->selectors) {
          if (selector->nodeType() != NodeType::RangeSelector) {
            continue;
          }
          auto rangeSelector = static_cast<const RangeSelector*>(selector);
          xml.openElement("RangeSelector");
          xml.addAttribute("start", rangeSelector->start, Default<RangeSelector>().start);
          xml.addAttribute("end", rangeSelector->end, Default<RangeSelector>().end);
          xml.addAttribute("offset", rangeSelector->offset, Default<RangeSelector>().offset);
          if (rangeSelector->unit != Default<RangeSelector>().unit) {
            xml.addAttribute("unit", SelectorUnitToString(rangeSelector->unit));
          }
          if (rangeSelector->shape != Default<RangeSelector>().shape) {
            xml.addAttribute("shape", SelectorShapeToString(rangeSelector->shape));
          }
          xml.addAttribute("easeIn", rangeSelector->easeIn, Default<RangeSelector>().easeIn);
          xml.addAttribute("easeOut", rangeSelector->easeOut, Default<RangeSelector>().easeOut);
          if (rangeSelector->mode != Default<RangeSelector>().mode) {
            xml.addAttribute("mode", SelectorModeToString(rangeSelector->mode));
          }
          xml.addAttribute("weight", rangeSelector->weight, Default<RangeSelector>().weight);
          xml.addAttribute("randomOrder", rangeSelector->randomOrder,
                           Default<RangeSelector>().randomOrder);
          xml.addAttribute("randomSeed", rangeSelector->randomSeed,
                           Default<RangeSelector>().randomSeed);
          WriteCustomData(xml, rangeSelector);
          xml.closeElementSelfClosing();
        }
        xml.closeElement();
      }
      break;
    }
    case NodeType::TextPath: {
      auto textPath = static_cast<const TextPath*>(node);
      xml.openElement("TextPath");
      if (textPath->path != nullptr && !textPath->path->id.empty()) {
        // Use the reference to PathData resource.
        xml.addAttribute("path", "@" + textPath->path->id);
      } else if (textPath->path != nullptr && !textPath->path->isEmpty()) {
        // Inline the path data.
        xml.addAttribute("path", PathDataToSVGString(*textPath->path));
      }
      if (textPath->baselineOrigin != Default<TextPath>().baselineOrigin) {
        xml.addAttribute("baselineOrigin", PointToString(textPath->baselineOrigin));
      }
      xml.addAttribute("baselineAngle", textPath->baselineAngle, Default<TextPath>().baselineAngle);
      xml.addAttribute("firstMargin", textPath->firstMargin, Default<TextPath>().firstMargin);
      xml.addAttribute("lastMargin", textPath->lastMargin, Default<TextPath>().lastMargin);
      xml.addAttribute("perpendicular", textPath->perpendicular, Default<TextPath>().perpendicular);
      xml.addAttribute("reversed", textPath->reversed, Default<TextPath>().reversed);
      xml.addAttribute("forceAlignment", textPath->forceAlignment,
                       Default<TextPath>().forceAlignment);
      xml.addOptionalAttribute("left", textPath->left);
      xml.addOptionalAttribute("right", textPath->right);
      xml.addOptionalAttribute("top", textPath->top);
      xml.addOptionalAttribute("bottom", textPath->bottom);
      xml.addOptionalAttribute("centerX", textPath->centerX);
      xml.addOptionalAttribute("centerY", textPath->centerY);
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::TextBox: {
      auto textBox = static_cast<const TextBox*>(node);
      xml.openElement("TextBox");
      // Group properties
      if (textBox->anchor != Default<TextBox>().anchor) {
        xml.addAttribute("anchor", PointToString(textBox->anchor));
      }
      if (!ShouldSkipPosition(textBox->position, {0, 0}, textBox->left, textBox->top,
                              textBox->right, textBox->bottom, textBox->centerX,
                              textBox->centerY)) {
        xml.addAttribute("position", PointToString(textBox->position));
      }
      xml.addAttribute("rotation", textBox->rotation, Default<TextBox>().rotation);
      if (textBox->scale != Default<TextBox>().scale) {
        xml.addAttribute("scale", PointToString(textBox->scale));
      }
      xml.addAttribute("skew", textBox->skew, Default<TextBox>().skew);
      xml.addAttribute("skewAxis", textBox->skewAxis, Default<TextBox>().skewAxis);
      xml.addAttribute("alpha", textBox->alpha, Default<TextBox>().alpha);
      // Layout dimensions
      xml.addOptionalAttribute("width", textBox->width);
      xml.addOptionalAttribute("height", textBox->height);
      if (!textBox->padding.isZero()) {
        xml.addAttribute("padding", PaddingToString(textBox->padding));
      }
      // TextBox typography properties
      if (textBox->textAlign != Default<TextBox>().textAlign) {
        xml.addAttribute("textAlign", TextAlignToString(textBox->textAlign));
      }
      if (textBox->paragraphAlign != Default<TextBox>().paragraphAlign) {
        xml.addAttribute("paragraphAlign", ParagraphAlignToString(textBox->paragraphAlign));
      }
      if (textBox->writingMode != Default<TextBox>().writingMode) {
        xml.addAttribute("writingMode", WritingModeToString(textBox->writingMode));
      }
      xml.addAttribute("lineHeight", textBox->lineHeight, Default<TextBox>().lineHeight);
      xml.addAttribute("wordWrap", textBox->wordWrap, Default<TextBox>().wordWrap);
      if (textBox->overflow != Default<TextBox>().overflow) {
        xml.addAttribute("overflow", OverflowToString(textBox->overflow));
      }
      // Constraint properties
      xml.addOptionalAttribute("left", textBox->left);
      xml.addOptionalAttribute("right", textBox->right);
      xml.addOptionalAttribute("top", textBox->top);
      xml.addOptionalAttribute("bottom", textBox->bottom);
      xml.addOptionalAttribute("centerX", textBox->centerX);
      xml.addOptionalAttribute("centerY", textBox->centerY);
      WriteCustomData(xml, node);
      // Child elements
      if (textBox->elements.empty()) {
        xml.closeElementSelfClosing();
      } else {
        xml.closeElementStart();
        for (const auto& element : textBox->elements) {
          WriteVectorElement(xml, element, options);
        }
        xml.closeElement();
      }
      break;
    }
    case NodeType::Repeater: {
      auto repeater = static_cast<const Repeater*>(node);
      xml.openElement("Repeater");
      xml.addAttribute("copies", repeater->copies, Default<Repeater>().copies);
      xml.addAttribute("offset", repeater->offset, Default<Repeater>().offset);
      if (repeater->order != Default<Repeater>().order) {
        xml.addAttribute("order", RepeaterOrderToString(repeater->order));
      }
      if (repeater->anchor != Default<Repeater>().anchor) {
        xml.addAttribute("anchor", PointToString(repeater->anchor));
      }
      if (repeater->position != Default<Repeater>().position) {
        xml.addAttribute("position", PointToString(repeater->position));
      }
      xml.addAttribute("rotation", repeater->rotation, Default<Repeater>().rotation);
      if (repeater->scale != Default<Repeater>().scale) {
        xml.addAttribute("scale", PointToString(repeater->scale));
      }
      xml.addAttribute("startAlpha", repeater->startAlpha, Default<Repeater>().startAlpha);
      xml.addAttribute("endAlpha", repeater->endAlpha, Default<Repeater>().endAlpha);
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::Group: {
      auto group = static_cast<const Group*>(node);
      xml.openElement("Group");
      if (group->anchor != Default<Group>().anchor) {
        xml.addAttribute("anchor", PointToString(group->anchor));
      }
      if (!ShouldSkipPosition(group->position, {0, 0}, group->left, group->top, group->right,
                              group->bottom, group->centerX, group->centerY)) {
        xml.addAttribute("position", PointToString(group->position));
      }
      xml.addAttribute("rotation", group->rotation, Default<Group>().rotation);
      if (group->scale != Default<Group>().scale) {
        xml.addAttribute("scale", PointToString(group->scale));
      }
      xml.addAttribute("skew", group->skew, Default<Group>().skew);
      xml.addAttribute("skewAxis", group->skewAxis, Default<Group>().skewAxis);
      xml.addAttribute("alpha", group->alpha, Default<Group>().alpha);
      xml.addOptionalAttribute("width", group->width);
      xml.addOptionalAttribute("height", group->height);
      if (!group->padding.isZero()) {
        xml.addAttribute("padding", PaddingToString(group->padding));
      }
      xml.addOptionalAttribute("left", group->left);
      xml.addOptionalAttribute("right", group->right);
      xml.addOptionalAttribute("top", group->top);
      xml.addOptionalAttribute("bottom", group->bottom);
      xml.addOptionalAttribute("centerX", group->centerX);
      xml.addOptionalAttribute("centerY", group->centerY);
      WriteCustomData(xml, node);
      if (group->elements.empty()) {
        xml.closeElementSelfClosing();
      } else {
        xml.closeElementStart();
        for (const auto& element : group->elements) {
          WriteVectorElement(xml, element, options);
        }
        xml.closeElement();
      }
      break;
    }
    default:
      break;
  }
}

//==============================================================================
// LayerStyle writing
//==============================================================================

static void WriteShadowAttributes(XMLBuilder& xml, float offsetX, float offsetY, float blurX,
                                  float blurY, const Color& color) {
  xml.addAttribute("offsetX", offsetX);
  xml.addAttribute("offsetY", offsetY);
  xml.addAttribute("blurX", blurX);
  xml.addAttribute("blurY", blurY);
  xml.addAttribute("color", ColorToHexString(color, color.alpha < 1.0f));
}

static void WriteLayerStyle(XMLBuilder& xml, const LayerStyle* node) {
  switch (node->nodeType()) {
    case NodeType::DropShadowStyle: {
      auto style = static_cast<const DropShadowStyle*>(node);
      xml.openElement("DropShadowStyle");
      if (style->blendMode != Default<DropShadowStyle>().blendMode) {
        xml.addAttribute("blendMode", BlendModeToString(style->blendMode));
      }
      xml.addAttribute("excludeChildEffects", style->excludeChildEffects,
                       Default<DropShadowStyle>().excludeChildEffects);
      WriteShadowAttributes(xml, style->offsetX, style->offsetY, style->blurX, style->blurY,
                            style->color);
      xml.addAttribute("showBehindLayer", style->showBehindLayer,
                       Default<DropShadowStyle>().showBehindLayer);
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::InnerShadowStyle: {
      auto style = static_cast<const InnerShadowStyle*>(node);
      xml.openElement("InnerShadowStyle");
      if (style->blendMode != Default<InnerShadowStyle>().blendMode) {
        xml.addAttribute("blendMode", BlendModeToString(style->blendMode));
      }
      xml.addAttribute("excludeChildEffects", style->excludeChildEffects,
                       Default<InnerShadowStyle>().excludeChildEffects);
      WriteShadowAttributes(xml, style->offsetX, style->offsetY, style->blurX, style->blurY,
                            style->color);
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::BackgroundBlurStyle: {
      auto style = static_cast<const BackgroundBlurStyle*>(node);
      xml.openElement("BackgroundBlurStyle");
      if (style->blendMode != Default<BackgroundBlurStyle>().blendMode) {
        xml.addAttribute("blendMode", BlendModeToString(style->blendMode));
      }
      xml.addAttribute("excludeChildEffects", style->excludeChildEffects,
                       Default<BackgroundBlurStyle>().excludeChildEffects);
      xml.addAttribute("blurX", style->blurX, Default<BackgroundBlurStyle>().blurX);
      xml.addAttribute("blurY", style->blurY, Default<BackgroundBlurStyle>().blurY);
      if (style->tileMode != Default<BackgroundBlurStyle>().tileMode) {
        xml.addAttribute("tileMode", TileModeToString(style->tileMode));
      }
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    default:
      break;
  }
}

//==============================================================================
// LayerFilter writing
//==============================================================================

static void WriteLayerFilter(XMLBuilder& xml, const LayerFilter* node) {
  switch (node->nodeType()) {
    case NodeType::BlurFilter: {
      auto filter = static_cast<const BlurFilter*>(node);
      xml.openElement("BlurFilter");
      xml.addRequiredAttribute("blurX", filter->blurX);
      xml.addRequiredAttribute("blurY", filter->blurY);
      if (filter->tileMode != Default<BlurFilter>().tileMode) {
        xml.addAttribute("tileMode", TileModeToString(filter->tileMode));
      }
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::DropShadowFilter: {
      auto filter = static_cast<const DropShadowFilter*>(node);
      xml.openElement("DropShadowFilter");
      WriteShadowAttributes(xml, filter->offsetX, filter->offsetY, filter->blurX, filter->blurY,
                            filter->color);
      xml.addAttribute("shadowOnly", filter->shadowOnly, Default<DropShadowFilter>().shadowOnly);
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::InnerShadowFilter: {
      auto filter = static_cast<const InnerShadowFilter*>(node);
      xml.openElement("InnerShadowFilter");
      WriteShadowAttributes(xml, filter->offsetX, filter->offsetY, filter->blurX, filter->blurY,
                            filter->color);
      xml.addAttribute("shadowOnly", filter->shadowOnly, Default<InnerShadowFilter>().shadowOnly);
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::BlendFilter: {
      auto filter = static_cast<const BlendFilter*>(node);
      xml.openElement("BlendFilter");
      xml.addAttribute("color", ColorToHexString(filter->color, filter->color.alpha < 1.0f));
      if (filter->blendMode != Default<BlendFilter>().blendMode) {
        xml.addAttribute("blendMode", BlendModeToString(filter->blendMode));
      }
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::ColorMatrixFilter: {
      auto filter = static_cast<const ColorMatrixFilter*>(node);
      xml.openElement("ColorMatrixFilter");
      xml.addAttribute("matrix", FloatListToString(filter->matrix.data(), filter->matrix.size()));
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    default:
      break;
  }
}

//==============================================================================
// Resource writing
//==============================================================================

static void WriteResource(XMLBuilder& xml, const Node* node, const Options& options) {
  switch (node->nodeType()) {
    case NodeType::Image: {
      auto image = static_cast<const Image*>(node);
      xml.openElement("Image");
      xml.addAttribute("id", image->id);
      if (image->data) {
        xml.addAttribute("source", "data:image/png;base64," +
                                       Base64Encode(image->data->bytes(), image->data->size()));
      } else {
        xml.addAttribute("source", image->filePath);
      }
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::PathData: {
      auto pathData = static_cast<const PathData*>(node);
      xml.openElement("PathData");
      xml.addAttribute("id", pathData->id);
      xml.addAttribute("data", PathDataToSVGString(*pathData));
      WriteCustomData(xml, node);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::Composition: {
      auto comp = static_cast<const Composition*>(node);
      xml.openElement("Composition");
      xml.addAttribute("id", comp->id);
      xml.addRequiredAttribute("width", comp->width);
      xml.addRequiredAttribute("height", comp->height);
      WriteCustomData(xml, node);
      if (comp->layers.empty()) {
        xml.closeElementSelfClosing();
      } else {
        xml.closeElementStart();
        for (const auto& layer : comp->layers) {
          WriteLayer(xml, layer, options);
        }
        xml.closeElement();
      }
      break;
    }
    case NodeType::Font: {
      // Skip Font resources if skipGlyphData is true
      if (options.skipGlyphData) {
        break;
      }
      auto font = static_cast<const Font*>(node);
      xml.openElement("Font");
      xml.addAttribute("id", font->id);
      xml.addAttribute("unitsPerEm", font->unitsPerEm, Default<Font>().unitsPerEm);
      WriteCustomData(xml, node);
      if (font->glyphs.empty()) {
        xml.closeElementSelfClosing();
      } else {
        xml.closeElementStart();
        for (const auto& glyph : font->glyphs) {
          xml.openElement("Glyph");
          xml.addAttribute("id", glyph->id);
          if (glyph->path != nullptr && !glyph->path->id.empty()) {
            xml.addAttribute("path", "@" + glyph->path->id);
          } else if (glyph->path != nullptr && !glyph->path->isEmpty()) {
            xml.addAttribute("path", PathDataToSVGString(*glyph->path));
          }
          if (glyph->image != nullptr) {
            if (!glyph->image->id.empty()) {
              xml.addAttribute("image", "@" + glyph->image->id);
            } else if (glyph->image->data) {
              xml.addAttribute("image",
                               "data:image/png;base64," + Base64Encode(glyph->image->data->bytes(),
                                                                       glyph->image->data->size()));
            } else if (!glyph->image->filePath.empty()) {
              xml.addAttribute("image", glyph->image->filePath);
            }
          }
          if (glyph->offset != Default<Glyph>().offset) {
            xml.addAttribute("offset", PointToString(glyph->offset));
          }
          xml.addRequiredAttribute("advance", glyph->advance);
          WriteCustomData(xml, glyph);
          xml.closeElementSelfClosing();
        }
        xml.closeElement();
      }
      break;
    }
    case NodeType::SolidColor:
    case NodeType::LinearGradient:
    case NodeType::RadialGradient:
    case NodeType::ConicGradient:
    case NodeType::DiamondGradient:
    case NodeType::ImagePattern: {
      WriteColorSource(xml, static_cast<const ColorSource*>(node));
      break;
    }
    default:
      break;
  }
}

//==============================================================================
// Layer writing
//==============================================================================

static void WriteLayer(XMLBuilder& xml, const Layer* node, const Options& options) {
  xml.openElement("Layer");
  if (!node->id.empty()) {
    xml.addAttribute("id", node->id);
  }
  xml.addAttribute("name", node->name);
  xml.addAttribute("visible", node->visible, Default<Layer>().visible);
  xml.addAttribute("alpha", node->alpha, Default<Layer>().alpha);
  if (node->blendMode != Default<Layer>().blendMode) {
    xml.addAttribute("blendMode", BlendModeToString(node->blendMode));
  }
  xml.addAttribute("x", node->x, Default<Layer>().x);
  xml.addAttribute("y", node->y, Default<Layer>().y);
  xml.addOptionalAttribute("width", node->width);
  xml.addOptionalAttribute("height", node->height);
  if (node->layout != Default<Layer>().layout) {
    xml.addAttribute("layout", LayoutModeToString(node->layout));
  }
  xml.addAttribute("gap", node->gap, Default<Layer>().gap);
  xml.addAttribute("flex", node->flex, Default<Layer>().flex);
  if (!node->padding.isZero()) {
    xml.addAttribute("padding", PaddingToString(node->padding));
  }
  if (node->alignment != Default<Layer>().alignment) {
    xml.addAttribute("alignment", AlignmentToString(node->alignment));
  }
  if (node->arrangement != Default<Layer>().arrangement) {
    xml.addAttribute("arrangement", ArrangementToString(node->arrangement));
  }
  xml.addAttribute("includeInLayout", node->includeInLayout, Default<Layer>().includeInLayout);
  xml.addOptionalAttribute("left", node->left);
  xml.addOptionalAttribute("right", node->right);
  xml.addOptionalAttribute("top", node->top);
  xml.addOptionalAttribute("bottom", node->bottom);
  xml.addOptionalAttribute("centerX", node->centerX);
  xml.addOptionalAttribute("centerY", node->centerY);
  if (!node->matrix.isIdentity()) {
    xml.addAttribute("matrix", MatrixToString(node->matrix));
  }
  if (!node->matrix3D.isIdentity()) {
    xml.addAttribute("matrix3D", FloatListToString(node->matrix3D.values, 16));
  }
  xml.addAttribute("preserve3D", node->preserve3D, Default<Layer>().preserve3D);
  xml.addAttribute("antiAlias", node->antiAlias, Default<Layer>().antiAlias);
  xml.addAttribute("groupOpacity", node->groupOpacity, Default<Layer>().groupOpacity);
  xml.addAttribute("passThroughBackground", node->passThroughBackground,
                   Default<Layer>().passThroughBackground);
  // Skip scrollRect when it was generated by clipToBounds during layout.
  // On re-import, clipToBounds will regenerate the correct scrollRect based on the new layout size.
  if (node->hasScrollRect && !node->clipToBounds) {
    xml.addAttribute("scrollRect", RectToString(node->scrollRect));
  }
  xml.addAttribute("clipToBounds", node->clipToBounds, Default<Layer>().clipToBounds);
  if (node->mask != nullptr && !node->mask->id.empty()) {
    xml.addAttribute("mask", "@" + node->mask->id);
  }
  if (node->maskType != Default<Layer>().maskType) {
    xml.addAttribute("maskType", MaskTypeToString(node->maskType));
  }
  if (node->composition != nullptr && !node->composition->id.empty()) {
    xml.addAttribute("composition", "@" + node->composition->id);
  }

  // Build directive attributes.
  xml.addAttribute("import", node->importDirective.source);
  xml.addAttribute("importFormat", node->importDirective.format);

  // Write custom data as data-* attributes.
  WriteCustomData(xml, node);

  bool hasChildren = !node->contents.empty() || !node->styles.empty() || !node->filters.empty() ||
                     !node->children.empty() || !node->importDirective.content.empty() ||
                     !node->importDirective.resolvedFrom.empty();
  if (!hasChildren) {
    xml.closeElementSelfClosing();
    return;
  }

  xml.closeElementStart();

  // Write resolved-from comment if present.
  if (!node->importDirective.resolvedFrom.empty()) {
    xml.writeComment("Resolved from: " + node->importDirective.resolvedFrom);
  }

  // Write inline import content (e.g., <svg>...</svg>) if present.
  if (!node->importDirective.content.empty()) {
    xml.writeRawLine(node->importDirective.content);
  }

  // Write VectorElement (contents) directly without container node.
  for (const auto& element : node->contents) {
    WriteVectorElement(xml, element, options);
  }

  // Write LayerStyle (styles) directly without container node.
  for (const auto& style : node->styles) {
    WriteLayerStyle(xml, style);
  }

  // Write LayerFilter (filters) directly without container node.
  for (const auto& filter : node->filters) {
    WriteLayerFilter(xml, filter);
  }

  // Write child Layers.
  for (const auto& child : node->children) {
    WriteLayer(xml, child, options);
  }

  xml.closeElement();
}

//==============================================================================
// Main Export function
//==============================================================================

std::string PAGXExporter::ToXML(const PAGXDocument& doc, const Options& options) {
  XMLBuilder xml = {};
  xml.appendDeclaration();

  xml.openElement("pagx");
  xml.addAttribute("version", doc.version);
  xml.addAttribute("width", doc.width);
  xml.addAttribute("height", doc.height);
  WriteCustomData(xml, &doc);
  xml.closeElementStart();

  // Write Layers first (for better readability)
  for (const auto& layer : doc.layers) {
    WriteLayer(xml, layer, options);
  }

  // Write Resources section at the end (only if there are exportable resources)
  bool hasResources = false;
  for (const auto& resource : doc.nodes) {
    if (!resource->id.empty()) {
      if (options.skipGlyphData && resource->nodeType() == NodeType::Font) {
        continue;
      }
      hasResources = true;
      break;
    }
  }
  if (hasResources) {
    xml.openElement("Resources");
    xml.closeElementStart();

    for (const auto& resource : doc.nodes) {
      if (!resource->id.empty()) {
        WriteResource(xml, resource.get(), options);
      }
    }

    xml.closeElement();
  }

  xml.closeElement();

  return xml.release();
}

}  // namespace pagx
