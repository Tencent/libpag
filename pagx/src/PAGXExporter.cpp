/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
#include <cstdio>
#include "Base64.h"
#include "StringParser.h"
#include "SVGPathParser.h"
#include "tgfx/core/Data.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorMatrixFilter.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
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
#include "pagx/nodes/TextLayout.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TrimPath.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"

namespace pagx {

//==============================================================================
// XMLBuilder - XML generation helper
//==============================================================================

class XMLBuilder {
 public:
  void appendDeclaration() {
    buffer += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  }

  void openElement(const std::string& tag) {
    writeIndent();
    buffer += "<";
    buffer += tag;
    tagStack.push_back(tag);
  }

  void addAttribute(const std::string& name, const std::string& value) {
    if (!value.empty()) {
      buffer += " ";
      buffer += name;
      buffer += "=\"";
      buffer += escapeXML(value);
      buffer += "\"";
    }
  }

  void addAttribute(const std::string& name, float value, float defaultValue = 0) {
    if (value != defaultValue) {
      buffer += " ";
      buffer += name;
      buffer += "=\"";
      buffer += FloatToString(value);
      buffer += "\"";
    }
  }

  void addRequiredAttribute(const std::string& name, float value) {
    buffer += " ";
    buffer += name;
    buffer += "=\"";
    buffer += FloatToString(value);
    buffer += "\"";
  }

  void addRequiredAttribute(const std::string& name, const std::string& value) {
    buffer += " ";
    buffer += name;
    buffer += "=\"";
    buffer += escapeXML(value);
    buffer += "\"";
  }

  void addAttribute(const std::string& name, int value, int defaultValue = 0) {
    if (value != defaultValue) {
      char buf[32] = {};
      snprintf(buf, sizeof(buf), "%d", value);
      buffer += " ";
      buffer += name;
      buffer += "=\"";
      buffer += buf;
      buffer += "\"";
    }
  }

  void addAttribute(const std::string& name, bool value, bool defaultValue = false) {
    if (value != defaultValue) {
      buffer += " ";
      buffer += name;
      buffer += "=\"";
      buffer += (value ? "true" : "false");
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

  void addTextContent(const std::string& text) {
    buffer += "<![CDATA[";
    buffer += text;
    buffer += "]]>";
  }

  const std::string& str() const {
    return buffer;
  }

 private:
  std::string buffer = {};
  std::vector<std::string> tagStack = {};
  int indentLevel = 0;

  void writeIndent() {
    for (int i = 0; i < indentLevel; i++) {
      buffer += "  ";
    }
  }

  static std::string escapeXML(const std::string& str) {
    std::string result = {};
    for (char c : str) {
      switch (c) {
        case '<':
          result += "&lt;";
          break;
        case '>':
          result += "&gt;";
          break;
        case '&':
          result += "&amp;";
          break;
        case '"':
          result += "&quot;";
          break;
        case '\'':
          result += "&apos;";
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

static std::string pointToString(const Point& p) {
  char buf[64] = {};
  snprintf(buf, sizeof(buf), "%g,%g", p.x, p.y);
  return std::string(buf);
}

static std::string sizeToString(const Size& s) {
  char buf[64] = {};
  snprintf(buf, sizeof(buf), "%g,%g", s.width, s.height);
  return std::string(buf);
}

static std::string rectToString(const Rect& r) {
  char buf[128] = {};
  snprintf(buf, sizeof(buf), "%g,%g,%g,%g", r.x, r.y, r.width, r.height);
  return std::string(buf);
}

static std::string floatListToString(const std::vector<float>& values) {
  std::string result;
  char buf[32] = {};
  for (size_t i = 0; i < values.size(); i++) {
    if (i > 0) {
      result += ",";
    }
    snprintf(buf, sizeof(buf), "%g", values[i]);
    result += buf;
  }
  return result;
}

//==============================================================================
// Forward declarations
//==============================================================================

using Options = PAGXExporter::Options;

static void writeColorSource(XMLBuilder& xml, const ColorSource* node);
static void writeVectorElement(XMLBuilder& xml, const Element* node, const Options& options);
static void writeLayerStyle(XMLBuilder& xml, const LayerStyle* node);
static void writeLayerFilter(XMLBuilder& xml, const LayerFilter* node);
static void writeResource(XMLBuilder& xml, const Node* node, const Options& options);
static void writeLayer(XMLBuilder& xml, const Layer* node, const Options& options);

//==============================================================================
// ColorStop and ColorSource writing
//==============================================================================

static void writeColorStops(XMLBuilder& xml, const std::vector<ColorStop>& stops) {
  for (const auto& stop : stops) {
    xml.openElement("ColorStop");
    xml.addRequiredAttribute("offset", stop.offset);
    xml.addRequiredAttribute("color", ColorToHexString(stop.color, stop.color.alpha < 1.0f));
    xml.closeElementSelfClosing();
  }
}

static void writeColorSource(XMLBuilder& xml, const ColorSource* node) {
  switch (node->nodeType()) {
    case NodeType::SolidColor: {
      auto solid = static_cast<const SolidColor*>(node);
      xml.openElement("SolidColor");
      xml.addAttribute("id", solid->id);
      xml.addAttribute("color", ColorToHexString(solid->color, solid->color.alpha < 1.0f));
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::LinearGradient: {
      auto grad = static_cast<const LinearGradient*>(node);
      xml.openElement("LinearGradient");
      xml.addAttribute("id", grad->id);
      if (grad->startPoint.x != 0 || grad->startPoint.y != 0) {
        xml.addAttribute("startPoint", pointToString(grad->startPoint));
      }
      xml.addRequiredAttribute("endPoint", pointToString(grad->endPoint));
      if (!grad->matrix.isIdentity()) {
        xml.addAttribute("matrix", MatrixToString(grad->matrix));
      }
      if (grad->colorStops.empty()) {
        xml.closeElementSelfClosing();
      } else {
        xml.closeElementStart();
        writeColorStops(xml, grad->colorStops);
        xml.closeElement();
      }
      break;
    }
    case NodeType::RadialGradient: {
      auto grad = static_cast<const RadialGradient*>(node);
      xml.openElement("RadialGradient");
      xml.addAttribute("id", grad->id);
      if (grad->center.x != 0 || grad->center.y != 0) {
        xml.addAttribute("center", pointToString(grad->center));
      }
      xml.addRequiredAttribute("radius", grad->radius);
      if (!grad->matrix.isIdentity()) {
        xml.addAttribute("matrix", MatrixToString(grad->matrix));
      }
      if (grad->colorStops.empty()) {
        xml.closeElementSelfClosing();
      } else {
        xml.closeElementStart();
        writeColorStops(xml, grad->colorStops);
        xml.closeElement();
      }
      break;
    }
    case NodeType::ConicGradient: {
      auto grad = static_cast<const ConicGradient*>(node);
      xml.openElement("ConicGradient");
      xml.addAttribute("id", grad->id);
      if (grad->center.x != 0 || grad->center.y != 0) {
        xml.addAttribute("center", pointToString(grad->center));
      }
      xml.addAttribute("startAngle", grad->startAngle);
      xml.addAttribute("endAngle", grad->endAngle, 360.0f);
      if (!grad->matrix.isIdentity()) {
        xml.addAttribute("matrix", MatrixToString(grad->matrix));
      }
      if (grad->colorStops.empty()) {
        xml.closeElementSelfClosing();
      } else {
        xml.closeElementStart();
        writeColorStops(xml, grad->colorStops);
        xml.closeElement();
      }
      break;
    }
    case NodeType::DiamondGradient: {
      auto grad = static_cast<const DiamondGradient*>(node);
      xml.openElement("DiamondGradient");
      xml.addAttribute("id", grad->id);
      if (grad->center.x != 0 || grad->center.y != 0) {
        xml.addAttribute("center", pointToString(grad->center));
      }
      xml.addRequiredAttribute("radius", grad->radius);
      if (!grad->matrix.isIdentity()) {
        xml.addAttribute("matrix", MatrixToString(grad->matrix));
      }
      if (grad->colorStops.empty()) {
        xml.closeElementSelfClosing();
      } else {
        xml.closeElementStart();
        writeColorStops(xml, grad->colorStops);
        xml.closeElement();
      }
      break;
    }
    case NodeType::ImagePattern: {
      auto pattern = static_cast<const ImagePattern*>(node);
      xml.openElement("ImagePattern");
      xml.addAttribute("id", pattern->id);
      if (pattern->image != nullptr && !pattern->image->id.empty()) {
        xml.addAttribute("image", "@" + pattern->image->id);
      }
      if (pattern->tileModeX != TileMode::Clamp) {
        xml.addAttribute("tileModeX", TileModeToString(pattern->tileModeX));
      }
      if (pattern->tileModeY != TileMode::Clamp) {
        xml.addAttribute("tileModeY", TileModeToString(pattern->tileModeY));
      }
      if (pattern->filterMode != FilterMode::Linear) {
        xml.addAttribute("filterMode", FilterModeToString(pattern->filterMode));
      }
      if (pattern->mipmapMode != MipmapMode::Linear) {
        xml.addAttribute("mipmapMode", MipmapModeToString(pattern->mipmapMode));
      }
      if (!pattern->matrix.isIdentity()) {
        xml.addAttribute("matrix", MatrixToString(pattern->matrix));
      }
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

static void writeVectorElement(XMLBuilder& xml, const Element* node, const Options& options) {
  switch (node->nodeType()) {
    case NodeType::Rectangle: {
      auto rect = static_cast<const Rectangle*>(node);
      xml.openElement("Rectangle");
      if (rect->center.x != 0 || rect->center.y != 0) {
        xml.addAttribute("center", pointToString(rect->center));
      }
      if (rect->size.width != 100 || rect->size.height != 100) {
        xml.addAttribute("size", sizeToString(rect->size));
      }
      xml.addAttribute("roundness", rect->roundness);
      xml.addAttribute("reversed", rect->reversed);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::Ellipse: {
      auto ellipse = static_cast<const Ellipse*>(node);
      xml.openElement("Ellipse");
      if (ellipse->center.x != 0 || ellipse->center.y != 0) {
        xml.addAttribute("center", pointToString(ellipse->center));
      }
      if (ellipse->size.width != 100 || ellipse->size.height != 100) {
        xml.addAttribute("size", sizeToString(ellipse->size));
      }
      xml.addAttribute("reversed", ellipse->reversed);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::Polystar: {
      auto polystar = static_cast<const Polystar*>(node);
      xml.openElement("Polystar");
      if (polystar->center.x != 0 || polystar->center.y != 0) {
        xml.addAttribute("center", pointToString(polystar->center));
      }
      xml.addAttribute("type", PolystarTypeToString(polystar->type));
      xml.addAttribute("pointCount", polystar->pointCount, 5.0f);
      xml.addAttribute("outerRadius", polystar->outerRadius, 100.0f);
      xml.addAttribute("innerRadius", polystar->innerRadius, 50.0f);
      xml.addAttribute("rotation", polystar->rotation);
      xml.addAttribute("outerRoundness", polystar->outerRoundness);
      xml.addAttribute("innerRoundness", polystar->innerRoundness);
      xml.addAttribute("reversed", polystar->reversed);
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
      xml.addAttribute("reversed", path->reversed);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::Text: {
      auto text = static_cast<const Text*>(node);
      xml.openElement("Text");
      if (!text->text.empty()) {
        xml.addAttribute("text", text->text);
      }
      if (text->position.x != 0 || text->position.y != 0) {
        xml.addAttribute("position", pointToString(text->position));
      }
      xml.addAttribute("fontFamily", text->fontFamily);
      if (!text->fontStyle.empty()) {
        xml.addAttribute("fontStyle", text->fontStyle);
      }
      xml.addAttribute("fontSize", text->fontSize, 12.0f);
      xml.addAttribute("letterSpacing", text->letterSpacing);
      xml.addAttribute("baselineShift", text->baselineShift);

      // Skip GlyphRuns if requested or if none exist
      if (options.skipGlyphData || text->glyphRuns.empty()) {
        xml.closeElementSelfClosing();
      } else {
        xml.closeElementStart();
        for (const auto& run : text->glyphRuns) {
          xml.openElement("GlyphRun");
          if (run->font != nullptr && !run->font->id.empty()) {
            xml.addAttribute("font", "@" + run->font->id);
          }
          xml.addAttribute("fontSize", run->fontSize, 12.0f);

          // Write glyphs as comma-separated integers
          if (!run->glyphs.empty()) {
            std::string glyphsStr = {};
            char buf[16] = {};
            for (size_t i = 0; i < run->glyphs.size(); i++) {
              if (i > 0) {
                glyphsStr += ",";
              }
              snprintf(buf, sizeof(buf), "%u", run->glyphs[i]);
              glyphsStr += buf;
            }
            xml.addRequiredAttribute("glyphs", glyphsStr);
          }

          // Determine positioning mode
          if (!run->matrices.empty()) {
            // Matrix mode: semicolon-separated groups of 6 values
            std::string matStr = {};
            char buf[32] = {};
            for (size_t i = 0; i < run->matrices.size(); i++) {
              if (i > 0) {
                matStr += ";";
              }
              const auto& m = run->matrices[i];
              snprintf(buf, sizeof(buf), "%g,%g,%g,%g,%g,%g", m.a, m.b, m.c, m.d, m.tx, m.ty);
              matStr += buf;
            }
            xml.addRequiredAttribute("matrices", matStr);
          } else if (!run->xforms.empty()) {
            // RSXform mode: semicolon-separated groups of 4 values
            std::string xformsStr = {};
            char buf[64] = {};
            for (size_t i = 0; i < run->xforms.size(); i++) {
              if (i > 0) {
                xformsStr += ";";
              }
              const auto& x = run->xforms[i];
              snprintf(buf, sizeof(buf), "%g,%g,%g,%g", x.scos, x.ssin, x.tx, x.ty);
              xformsStr += buf;
            }
            xml.addRequiredAttribute("xforms", xformsStr);
          } else if (!run->positions.empty()) {
            // Point mode: semicolon-separated x,y pairs
            std::string posStr = {};
            char buf[32] = {};
            for (size_t i = 0; i < run->positions.size(); i++) {
              if (i > 0) {
                posStr += ";";
              }
              snprintf(buf, sizeof(buf), "%g,%g", run->positions[i].x, run->positions[i].y);
              posStr += buf;
            }
            xml.addRequiredAttribute("positions", posStr);
          } else if (!run->xPositions.empty()) {
            // Horizontal mode
            xml.addAttribute("y", run->y);
            xml.addRequiredAttribute("xPositions", floatListToString(run->xPositions));
          }

          xml.closeElementSelfClosing();
        }
        xml.closeElement();
      }
      break;
    }
    case NodeType::Fill: {
      auto fill = static_cast<const Fill*>(node);
      xml.openElement("Fill");
      // Determine color attribute value
      bool needsInlineColorSource = false;
      if (fill->color) {
        if (!fill->color->id.empty()) {
          // Reference to resource by id
          xml.addAttribute("color", "@" + fill->color->id);
        } else if (fill->color->nodeType() == NodeType::SolidColor) {
          // SolidColor without id: output color value directly
          auto solid = static_cast<const SolidColor*>(fill->color);
          xml.addAttribute("color",
                           ColorToHexString(solid->color, solid->color.alpha < 1.0f));
        } else {
          // Other ColorSource without id: needs inline as child element
          needsInlineColorSource = true;
        }
      }
      xml.addAttribute("alpha", fill->alpha, 1.0f);
      if (fill->blendMode != BlendMode::Normal) {
        xml.addAttribute("blendMode", BlendModeToString(fill->blendMode));
      }
      if (fill->fillRule != FillRule::Winding) {
        xml.addAttribute("fillRule", FillRuleToString(fill->fillRule));
      }
      if (fill->placement != LayerPlacement::Background) {
        xml.addAttribute("placement", LayerPlacementToString(fill->placement));
      }
      if (needsInlineColorSource) {
        xml.closeElementStart();
        writeColorSource(xml, fill->color);
        xml.closeElement();
      } else {
        xml.closeElementSelfClosing();
      }
      break;
    }
    case NodeType::Stroke: {
      auto stroke = static_cast<const Stroke*>(node);
      xml.openElement("Stroke");
      // Determine color attribute value
      bool needsInlineColorSource = false;
      if (stroke->color) {
        if (!stroke->color->id.empty()) {
          // Reference to resource by id
          xml.addAttribute("color", "@" + stroke->color->id);
        } else if (stroke->color->nodeType() == NodeType::SolidColor) {
          // SolidColor without id: output color value directly
          auto solid = static_cast<const SolidColor*>(stroke->color);
          xml.addAttribute("color",
                           ColorToHexString(solid->color, solid->color.alpha < 1.0f));
        } else {
          // Other ColorSource without id: needs inline as child element
          needsInlineColorSource = true;
        }
      }
      xml.addAttribute("width", stroke->width, 1.0f);
      xml.addAttribute("alpha", stroke->alpha, 1.0f);
      if (stroke->blendMode != BlendMode::Normal) {
        xml.addAttribute("blendMode", BlendModeToString(stroke->blendMode));
      }
      if (stroke->cap != LineCap::Butt) {
        xml.addAttribute("cap", LineCapToString(stroke->cap));
      }
      if (stroke->join != LineJoin::Miter) {
        xml.addAttribute("join", LineJoinToString(stroke->join));
      }
      xml.addAttribute("miterLimit", stroke->miterLimit, 4.0f);
      if (!stroke->dashes.empty()) {
        xml.addAttribute("dashes", floatListToString(stroke->dashes));
      }
      xml.addAttribute("dashOffset", stroke->dashOffset);
      if (stroke->align != StrokeAlign::Center) {
        xml.addAttribute("align", StrokeAlignToString(stroke->align));
      }
      if (stroke->placement != LayerPlacement::Background) {
        xml.addAttribute("placement", LayerPlacementToString(stroke->placement));
      }
      if (needsInlineColorSource) {
        xml.closeElementStart();
        writeColorSource(xml, stroke->color);
        xml.closeElement();
      } else {
        xml.closeElementSelfClosing();
      }
      break;
    }
    case NodeType::TrimPath: {
      auto trim = static_cast<const TrimPath*>(node);
      xml.openElement("TrimPath");
      xml.addAttribute("start", trim->start);
      xml.addAttribute("end", trim->end, 1.0f);
      xml.addAttribute("offset", trim->offset);
      if (trim->type != TrimType::Separate) {
        xml.addAttribute("type", TrimTypeToString(trim->type));
      }
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::RoundCorner: {
      auto round = static_cast<const RoundCorner*>(node);
      xml.openElement("RoundCorner");
      xml.addAttribute("radius", round->radius, 10.0f);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::MergePath: {
      auto merge = static_cast<const MergePath*>(node);
      xml.openElement("MergePath");
      if (merge->mode != MergePathMode::Append) {
        xml.addAttribute("mode", MergePathModeToString(merge->mode));
      }
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::TextModifier: {
      auto modifier = static_cast<const TextModifier*>(node);
      xml.openElement("TextModifier");
      if (modifier->anchorPoint.x != 0 || modifier->anchorPoint.y != 0) {
        xml.addAttribute("anchorPoint", pointToString(modifier->anchorPoint));
      }
      if (modifier->position.x != 0 || modifier->position.y != 0) {
        xml.addAttribute("position", pointToString(modifier->position));
      }
      xml.addAttribute("rotation", modifier->rotation);
      if (modifier->scale.x != 1 || modifier->scale.y != 1) {
        xml.addAttribute("scale", pointToString(modifier->scale));
      }
      xml.addAttribute("skew", modifier->skew);
      xml.addAttribute("skewAxis", modifier->skewAxis);
      xml.addAttribute("alpha", modifier->alpha, 1.0f);
      if (modifier->fillColor.has_value()) {
        xml.addAttribute("fillColor", ColorToHexString(modifier->fillColor.value()));
      }
      if (modifier->strokeColor.has_value()) {
        xml.addAttribute("strokeColor", ColorToHexString(modifier->strokeColor.value()));
      }
      if (modifier->strokeWidth.has_value()) {
        xml.addAttribute("strokeWidth", modifier->strokeWidth.value());
      }
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
          xml.addAttribute("start", rangeSelector->start);
          xml.addAttribute("end", rangeSelector->end, 1.0f);
          xml.addAttribute("offset", rangeSelector->offset);
          if (rangeSelector->unit != SelectorUnit::Percentage) {
            xml.addAttribute("unit", SelectorUnitToString(rangeSelector->unit));
          }
          if (rangeSelector->shape != SelectorShape::Square) {
            xml.addAttribute("shape", SelectorShapeToString(rangeSelector->shape));
          }
          xml.addAttribute("easeIn", rangeSelector->easeIn);
          xml.addAttribute("easeOut", rangeSelector->easeOut);
          if (rangeSelector->mode != SelectorMode::Add) {
            xml.addAttribute("mode", SelectorModeToString(rangeSelector->mode));
          }
          xml.addAttribute("weight", rangeSelector->weight, 1.0f);
          xml.addAttribute("randomOrder", rangeSelector->randomOrder);
          xml.addAttribute("randomSeed", rangeSelector->randomSeed);
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
      if (textPath->textAlign != TextAlign::Start) {
        xml.addAttribute("textAlign", TextAlignToString(textPath->textAlign));
      }
      xml.addAttribute("firstMargin", textPath->firstMargin);
      xml.addAttribute("lastMargin", textPath->lastMargin);
      xml.addAttribute("perpendicular", textPath->perpendicular, true);
      xml.addAttribute("reversed", textPath->reversed);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::TextLayout: {
      auto layout = static_cast<const TextLayout*>(node);
      xml.openElement("TextLayout");
      if (layout->position.x != 0 || layout->position.y != 0) {
        xml.addAttribute("position", pointToString(layout->position));
      }
      xml.addAttribute("width", layout->width);
      xml.addAttribute("height", layout->height);
      if (layout->textAlign != TextAlign::Start) {
        xml.addAttribute("textAlign", TextAlignToString(layout->textAlign));
      }
      if (layout->verticalAlign != VerticalAlign::Top) {
        xml.addAttribute("verticalAlign", VerticalAlignToString(layout->verticalAlign));
      }
      if (layout->writingMode != WritingMode::Horizontal) {
        xml.addAttribute("writingMode", WritingModeToString(layout->writingMode));
      }
      xml.addAttribute("lineHeight", layout->lineHeight, 1.2f);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::Repeater: {
      auto repeater = static_cast<const Repeater*>(node);
      xml.openElement("Repeater");
      xml.addAttribute("copies", repeater->copies, 3.0f);
      xml.addAttribute("offset", repeater->offset);
      if (repeater->order != RepeaterOrder::BelowOriginal) {
        xml.addAttribute("order", RepeaterOrderToString(repeater->order));
      }
      if (repeater->anchorPoint.x != 0 || repeater->anchorPoint.y != 0) {
        xml.addAttribute("anchorPoint", pointToString(repeater->anchorPoint));
      }
      if (repeater->position.x != 100 || repeater->position.y != 100) {
        xml.addAttribute("position", pointToString(repeater->position));
      }
      xml.addAttribute("rotation", repeater->rotation);
      if (repeater->scale.x != 1 || repeater->scale.y != 1) {
        xml.addAttribute("scale", pointToString(repeater->scale));
      }
      xml.addAttribute("startAlpha", repeater->startAlpha, 1.0f);
      xml.addAttribute("endAlpha", repeater->endAlpha, 1.0f);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::Group: {
      auto group = static_cast<const Group*>(node);
      xml.openElement("Group");
      if (group->anchorPoint.x != 0 || group->anchorPoint.y != 0) {
        xml.addAttribute("anchorPoint", pointToString(group->anchorPoint));
      }
      if (group->position.x != 0 || group->position.y != 0) {
        xml.addAttribute("position", pointToString(group->position));
      }
      xml.addAttribute("rotation", group->rotation);
      if (group->scale.x != 1 || group->scale.y != 1) {
        xml.addAttribute("scale", pointToString(group->scale));
      }
      xml.addAttribute("skew", group->skew);
      xml.addAttribute("skewAxis", group->skewAxis);
      xml.addAttribute("alpha", group->alpha, 1.0f);
      if (group->elements.empty()) {
        xml.closeElementSelfClosing();
      } else {
        xml.closeElementStart();
        for (const auto& element : group->elements) {
          writeVectorElement(xml, element, options);
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

static void writeLayerStyle(XMLBuilder& xml, const LayerStyle* node) {
  switch (node->nodeType()) {
    case NodeType::DropShadowStyle: {
      auto style = static_cast<const DropShadowStyle*>(node);
      xml.openElement("DropShadowStyle");
      if (style->blendMode != BlendMode::Normal) {
        xml.addAttribute("blendMode", BlendModeToString(style->blendMode));
      }
      xml.addAttribute("offsetX", style->offsetX);
      xml.addAttribute("offsetY", style->offsetY);
      xml.addAttribute("blurX", style->blurX);
      xml.addAttribute("blurY", style->blurY);
      xml.addAttribute("color", ColorToHexString(style->color, style->color.alpha < 1.0f));
      xml.addAttribute("showBehindLayer", style->showBehindLayer, true);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::InnerShadowStyle: {
      auto style = static_cast<const InnerShadowStyle*>(node);
      xml.openElement("InnerShadowStyle");
      if (style->blendMode != BlendMode::Normal) {
        xml.addAttribute("blendMode", BlendModeToString(style->blendMode));
      }
      xml.addAttribute("offsetX", style->offsetX);
      xml.addAttribute("offsetY", style->offsetY);
      xml.addAttribute("blurX", style->blurX);
      xml.addAttribute("blurY", style->blurY);
      xml.addAttribute("color", ColorToHexString(style->color, style->color.alpha < 1.0f));
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::BackgroundBlurStyle: {
      auto style = static_cast<const BackgroundBlurStyle*>(node);
      xml.openElement("BackgroundBlurStyle");
      if (style->blendMode != BlendMode::Normal) {
        xml.addAttribute("blendMode", BlendModeToString(style->blendMode));
      }
      xml.addAttribute("blurX", style->blurX);
      xml.addAttribute("blurY", style->blurY);
      if (style->tileMode != TileMode::Mirror) {
        xml.addAttribute("tileMode", TileModeToString(style->tileMode));
      }
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

static void writeLayerFilter(XMLBuilder& xml, const LayerFilter* node) {
  switch (node->nodeType()) {
    case NodeType::BlurFilter: {
      auto filter = static_cast<const BlurFilter*>(node);
      xml.openElement("BlurFilter");
      xml.addRequiredAttribute("blurX", filter->blurX);
      xml.addRequiredAttribute("blurY", filter->blurY);
      if (filter->tileMode != TileMode::Decal) {
        xml.addAttribute("tileMode", TileModeToString(filter->tileMode));
      }
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::DropShadowFilter: {
      auto filter = static_cast<const DropShadowFilter*>(node);
      xml.openElement("DropShadowFilter");
      xml.addAttribute("offsetX", filter->offsetX);
      xml.addAttribute("offsetY", filter->offsetY);
      xml.addAttribute("blurX", filter->blurX);
      xml.addAttribute("blurY", filter->blurY);
      xml.addAttribute("color", ColorToHexString(filter->color, filter->color.alpha < 1.0f));
      xml.addAttribute("shadowOnly", filter->shadowOnly);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::InnerShadowFilter: {
      auto filter = static_cast<const InnerShadowFilter*>(node);
      xml.openElement("InnerShadowFilter");
      xml.addAttribute("offsetX", filter->offsetX);
      xml.addAttribute("offsetY", filter->offsetY);
      xml.addAttribute("blurX", filter->blurX);
      xml.addAttribute("blurY", filter->blurY);
      xml.addAttribute("color", ColorToHexString(filter->color, filter->color.alpha < 1.0f));
      xml.addAttribute("shadowOnly", filter->shadowOnly);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::BlendFilter: {
      auto filter = static_cast<const BlendFilter*>(node);
      xml.openElement("BlendFilter");
      xml.addAttribute("color", ColorToHexString(filter->color, filter->color.alpha < 1.0f));
      if (filter->blendMode != BlendMode::Normal) {
        xml.addAttribute("blendMode", BlendModeToString(filter->blendMode));
      }
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::ColorMatrixFilter: {
      auto filter = static_cast<const ColorMatrixFilter*>(node);
      xml.openElement("ColorMatrixFilter");
      std::vector<float> values(filter->matrix.begin(), filter->matrix.end());
      xml.addAttribute("matrix", floatListToString(values));
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

static void writeResource(XMLBuilder& xml, const Node* node, const Options& options) {
  switch (node->nodeType()) {
    case NodeType::Image: {
      auto image = static_cast<const Image*>(node);
      xml.openElement("Image");
      xml.addAttribute("id", image->id);
      if (image->data) {
        xml.addAttribute("source", "data:image/png;base64," +
                                       Base64Encode(reinterpret_cast<const uint8_t*>(image->data->data()), image->data->size()));
      } else {
        xml.addAttribute("source", image->filePath);
      }
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::PathData: {
      auto pathData = static_cast<const PathData*>(node);
      xml.openElement("PathData");
      xml.addAttribute("id", pathData->id);
      xml.addAttribute("data", PathDataToSVGString(*pathData));
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::Composition: {
      auto comp = static_cast<const Composition*>(node);
      xml.openElement("Composition");
      xml.addAttribute("id", comp->id);
      xml.addRequiredAttribute("width", static_cast<float>(comp->width));
      xml.addRequiredAttribute("height", static_cast<float>(comp->height));
      if (comp->layers.empty()) {
        xml.closeElementSelfClosing();
      } else {
        xml.closeElementStart();
        for (const auto& layer : comp->layers) {
          writeLayer(xml, layer, options);
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
      xml.addAttribute("unitsPerEm", font->unitsPerEm);
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
              xml.addAttribute("image", "data:image/png;base64," +
                                            Base64Encode(glyph->image->data->bytes(),
                                                         glyph->image->data->size()));
            } else if (!glyph->image->filePath.empty()) {
              xml.addAttribute("image", glyph->image->filePath);
            }
          }
          if (glyph->offset.x != 0 || glyph->offset.y != 0) {
            xml.addAttribute("offset", pointToString(glyph->offset));
          }
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
      writeColorSource(xml, static_cast<const ColorSource*>(node));
      break;
    }
    default:
      break;
  }
}

//==============================================================================
// Layer writing
//==============================================================================

static void writeLayer(XMLBuilder& xml, const Layer* node, const Options& options) {
  xml.openElement("Layer");
  if (!node->id.empty()) {
    xml.addAttribute("id", node->id);
  }
  xml.addAttribute("name", node->name);
  xml.addAttribute("visible", node->visible, true);
  xml.addAttribute("alpha", node->alpha, 1.0f);
  if (node->blendMode != BlendMode::Normal) {
    xml.addAttribute("blendMode", BlendModeToString(node->blendMode));
  }
  xml.addAttribute("x", node->x);
  xml.addAttribute("y", node->y);
  if (!node->matrix.isIdentity()) {
    xml.addAttribute("matrix", MatrixToString(node->matrix));
  }
  if (!node->matrix3D.empty()) {
    xml.addAttribute("matrix3D", floatListToString(node->matrix3D));
  }
  xml.addAttribute("preserve3D", node->preserve3D);
  xml.addAttribute("antiAlias", node->antiAlias, true);
  xml.addAttribute("groupOpacity", node->groupOpacity);
  xml.addAttribute("passThroughBackground", node->passThroughBackground, true);
  xml.addAttribute("excludeChildEffectsInLayerStyle", node->excludeChildEffectsInLayerStyle);
  if (node->hasScrollRect) {
    xml.addAttribute("scrollRect", rectToString(node->scrollRect));
  }
  if (node->mask != nullptr && !node->mask->id.empty()) {
    xml.addAttribute("mask", "@" + node->mask->id);
  }
  if (node->maskType != MaskType::Alpha) {
    xml.addAttribute("maskType", MaskTypeToString(node->maskType));
  }
  if (node->composition != nullptr && !node->composition->id.empty()) {
    xml.addAttribute("composition", "@" + node->composition->id);
  }

  // Write custom data as data-* attributes.
  for (const auto& [key, value] : node->customData) {
    xml.addAttribute("data-" + key, value);
  }

  bool hasChildren = !node->contents.empty() || !node->styles.empty() || !node->filters.empty() ||
                     !node->children.empty();
  if (!hasChildren) {
    xml.closeElementSelfClosing();
    return;
  }

  xml.closeElementStart();

  // Write VectorElement (contents) directly without container node.
  for (const auto& element : node->contents) {
    writeVectorElement(xml, element, options);
  }

  // Write LayerStyle (styles) directly without container node.
  for (const auto& style : node->styles) {
    writeLayerStyle(xml, style);
  }

  // Write LayerFilter (filters) directly without container node.
  for (const auto& filter : node->filters) {
    writeLayerFilter(xml, filter);
  }

  // Write child Layers.
  for (const auto& child : node->children) {
    writeLayer(xml, child, options);
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
  xml.closeElementStart();

  // Write Layers first (for better readability)
  for (const auto& layer : doc.layers) {
    writeLayer(xml, layer, options);
  }

  // Write Resources section at the end (only if there are resources)
  if (!doc.nodes.empty()) {
    xml.openElement("Resources");
    xml.closeElementStart();

    for (const auto& resource : doc.nodes) {
      if (!resource->id.empty()) {
        writeResource(xml, resource.get(), options);
      }
    }

    xml.closeElement();
  }

  xml.closeElement();

  return xml.str();
}

}  // namespace pagx
