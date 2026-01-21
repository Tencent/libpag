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

#include "PAGXXMLWriter.h"
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include "pagx/model/BackgroundBlurStyle.h"
#include "pagx/model/BlendFilter.h"
#include "pagx/model/BlurFilter.h"
#include "pagx/model/ColorMatrixFilter.h"
#include "pagx/model/Composition.h"
#include "pagx/model/ConicGradient.h"
#include "pagx/model/DiamondGradient.h"
#include "pagx/model/Document.h"
#include "pagx/model/DropShadowFilter.h"
#include "pagx/model/DropShadowStyle.h"
#include "pagx/model/Ellipse.h"
#include "pagx/model/Fill.h"
#include "pagx/model/Group.h"
#include "pagx/model/Image.h"
#include "pagx/model/ImagePattern.h"
#include "pagx/model/InnerShadowFilter.h"
#include "pagx/model/InnerShadowStyle.h"
#include "pagx/model/LinearGradient.h"
#include "pagx/model/MergePath.h"
#include "pagx/model/Path.h"
#include "pagx/model/PathDataResource.h"
#include "pagx/model/Polystar.h"
#include "pagx/model/RadialGradient.h"
#include "pagx/model/RangeSelector.h"
#include "pagx/model/Rectangle.h"
#include "pagx/model/Repeater.h"
#include "pagx/model/RoundCorner.h"
#include "pagx/model/SolidColor.h"
#include "pagx/model/Stroke.h"
#include "pagx/model/TextLayout.h"
#include "pagx/model/TextModifier.h"
#include "pagx/model/TextPath.h"
#include "pagx/model/TextSpan.h"
#include "pagx/model/TrimPath.h"

namespace pagx {

//==============================================================================
// XMLBuilder - XML generation helper
//==============================================================================

class XMLBuilder {
 public:
  void appendDeclaration() {
    buffer << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  }

  void openElement(const std::string& tag) {
    writeIndent();
    buffer << "<" << tag;
    tagStack.push_back(tag);
  }

  void addAttribute(const std::string& name, const std::string& value) {
    if (!value.empty()) {
      buffer << " " << name << "=\"" << escapeXML(value) << "\"";
    }
  }

  void addAttribute(const std::string& name, float value, float defaultValue = 0) {
    if (value != defaultValue) {
      buffer << " " << name << "=\"" << formatFloat(value) << "\"";
    }
  }

  void addRequiredAttribute(const std::string& name, float value) {
    buffer << " " << name << "=\"" << formatFloat(value) << "\"";
  }

  void addRequiredAttribute(const std::string& name, const std::string& value) {
    buffer << " " << name << "=\"" << escapeXML(value) << "\"";
  }

  void addAttribute(const std::string& name, int value, int defaultValue = 0) {
    if (value != defaultValue) {
      buffer << " " << name << "=\"" << value << "\"";
    }
  }

  void addAttribute(const std::string& name, bool value, bool defaultValue = false) {
    if (value != defaultValue) {
      buffer << " " << name << "=\"" << (value ? "true" : "false") << "\"";
    }
  }

  void closeElementStart() {
    buffer << ">\n";
    indentLevel++;
  }

  void closeElementSelfClosing() {
    buffer << "/>\n";
    tagStack.pop_back();
  }

  void closeElement() {
    indentLevel--;
    writeIndent();
    buffer << "</" << tagStack.back() << ">\n";
    tagStack.pop_back();
  }

  void addTextContent(const std::string& text) {
    buffer << "<![CDATA[" << text << "]]>";
  }

  std::string str() const {
    return buffer.str();
  }

 private:
  std::ostringstream buffer = {};
  std::vector<std::string> tagStack = {};
  int indentLevel = 0;

  void writeIndent() {
    for (int i = 0; i < indentLevel; i++) {
      buffer << "  ";
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

  static std::string formatFloat(float value) {
    std::ostringstream oss = {};
    oss << value;
    auto str = oss.str();
    if (str.find('.') != std::string::npos) {
      while (str.back() == '0') {
        str.pop_back();
      }
      if (str.back() == '.') {
        str.pop_back();
      }
    }
    return str;
  }
};

//==============================================================================
// Helper functions for converting types to strings
//==============================================================================

static std::string pointToString(const Point& p) {
  std::ostringstream oss = {};
  oss << p.x << "," << p.y;
  return oss.str();
}

static std::string sizeToString(const Size& s) {
  std::ostringstream oss = {};
  oss << s.width << "," << s.height;
  return oss.str();
}

static std::string rectToString(const Rect& r) {
  std::ostringstream oss = {};
  oss << r.x << "," << r.y << "," << r.width << "," << r.height;
  return oss.str();
}

static std::string floatListToString(const std::vector<float>& values) {
  std::ostringstream oss = {};
  for (size_t i = 0; i < values.size(); i++) {
    if (i > 0) {
      oss << ",";
    }
    oss << values[i];
  }
  return oss.str();
}

//==============================================================================
// ColorSource serialization helper
//==============================================================================

static std::string colorSourceToKey(const ColorSource* node) {
  if (!node) {
    return "";
  }
  std::ostringstream oss = {};
  switch (node->type()) {
    case ColorSourceType::SolidColor: {
      auto solid = static_cast<const SolidColor*>(node);
      oss << "SolidColor:" << solid->color.toHexString(true);
      break;
    }
    case ColorSourceType::LinearGradient: {
      auto grad = static_cast<const LinearGradient*>(node);
      oss << "LinearGradient:" << grad->startPoint.x << "," << grad->startPoint.y << ":"
          << grad->endPoint.x << "," << grad->endPoint.y << ":" << grad->matrix.toString() << ":";
      for (const auto& stop : grad->colorStops) {
        oss << stop.offset << "=" << stop.color.toHexString(true) << ";";
      }
      break;
    }
    case ColorSourceType::RadialGradient: {
      auto grad = static_cast<const RadialGradient*>(node);
      oss << "RadialGradient:" << grad->center.x << "," << grad->center.y << ":" << grad->radius
          << ":" << grad->matrix.toString() << ":";
      for (const auto& stop : grad->colorStops) {
        oss << stop.offset << "=" << stop.color.toHexString(true) << ";";
      }
      break;
    }
    case ColorSourceType::ConicGradient: {
      auto grad = static_cast<const ConicGradient*>(node);
      oss << "ConicGradient:" << grad->center.x << "," << grad->center.y << ":" << grad->startAngle
          << ":" << grad->endAngle << ":" << grad->matrix.toString() << ":";
      for (const auto& stop : grad->colorStops) {
        oss << stop.offset << "=" << stop.color.toHexString(true) << ";";
      }
      break;
    }
    case ColorSourceType::DiamondGradient: {
      auto grad = static_cast<const DiamondGradient*>(node);
      oss << "DiamondGradient:" << grad->center.x << "," << grad->center.y << ":"
          << grad->halfDiagonal << ":" << grad->matrix.toString() << ":";
      for (const auto& stop : grad->colorStops) {
        oss << stop.offset << "=" << stop.color.toHexString(true) << ";";
      }
      break;
    }
    case ColorSourceType::ImagePattern: {
      auto pattern = static_cast<const ImagePattern*>(node);
      oss << "ImagePattern:" << pattern->image << ":" << static_cast<int>(pattern->tileModeX) << ":"
          << static_cast<int>(pattern->tileModeY) << ":" << static_cast<int>(pattern->sampling)
          << ":" << pattern->matrix.toString();
      break;
    }
  }
  return oss.str();
}

//==============================================================================
// ResourceContext - tracks extracted resources and reference counts
//==============================================================================

class ResourceContext {
 public:
  // PathData: SVG string -> resource id
  std::unordered_map<std::string, std::string> pathDataMap = {};

  // ColorSource: serialized key -> (resource id, reference count)
  std::unordered_map<std::string, std::pair<std::string, int>> colorSourceMap = {};

  // All extracted PathData resources (ordered)
  std::vector<std::pair<std::string, std::string>> pathDataResources = {};  // id -> svg string

  // All extracted ColorSource resources (ordered)
  std::vector<std::pair<std::string, const ColorSource*>> colorSourceResources = {};;

  int nextPathId = 1;
  int nextColorId = 1;

  // First pass: collect and count all resources
  void collectFromDocument(const Document& doc) {
    for (const auto& layer : doc.layers) {
      collectFromLayer(layer.get());
    }
    for (const auto& resource : doc.resources) {
      if (resource->nodeType() == NodeType::Composition) {
        auto comp = static_cast<const Composition*>(resource.get());
        for (const auto& layer : comp->layers) {
          collectFromLayer(layer.get());
        }
      }
    }
  }

  // Get or create PathData resource id
  std::string getPathDataId(const std::string& svgString) {
    auto it = pathDataMap.find(svgString);
    if (it != pathDataMap.end()) {
      return it->second;
    }
    std::string id = "path" + std::to_string(nextPathId++);
    pathDataMap[svgString] = id;
    pathDataResources.emplace_back(id, svgString);
    return id;
  }

  // Register ColorSource usage (for counting)
  void registerColorSource(const ColorSource* node) {
    if (!node) {
      return;
    }
    std::string key = colorSourceToKey(node);
    if (key.empty()) {
      return;
    }
    auto it = colorSourceMap.find(key);
    if (it != colorSourceMap.end()) {
      it->second.second++;  // Increment reference count
    } else {
      colorSourceMap[key] = {"", 1};  // Will assign id later if needed
    }
  }

  // Finalize: assign ids to ColorSources with multiple references
  void finalizeColorSources() {
    for (auto& [key, value] : colorSourceMap) {
      if (value.second > 1) {
        value.first = "color" + std::to_string(nextColorId++);
      }
    }
  }

  // Check if ColorSource should be extracted to Resources
  bool shouldExtractColorSource(const ColorSource* node) const {
    if (!node) {
      return false;
    }
    std::string key = colorSourceToKey(node);
    auto it = colorSourceMap.find(key);
    return it != colorSourceMap.end() && it->second.second > 1;
  }

  // Get ColorSource resource id (empty if should inline)
  std::string getColorSourceId(const ColorSource* node) const {
    if (!node) {
      return "";
    }
    std::string key = colorSourceToKey(node);
    auto it = colorSourceMap.find(key);
    if (it != colorSourceMap.end() && it->second.second > 1) {
      return it->second.first;
    }
    return "";
  }

 private:
  void collectFromLayer(const Layer* layer) {
    for (const auto& element : layer->contents) {
      collectFromVectorElement(element.get());
    }
    for (const auto& child : layer->children) {
      collectFromLayer(child.get());
    }
  }

  void collectFromVectorElement(const Element* element) {
    switch (element->type()) {
      case ElementType::Path: {
        auto path = static_cast<const Path*>(element);
        if (!path->data.isEmpty()) {
          getPathDataId(path->data.toSVGString());
        }
        break;
      }
      case ElementType::Fill: {
        auto fill = static_cast<const Fill*>(element);
        if (fill->colorSource) {
          registerColorSource(fill->colorSource.get());
        }
        break;
      }
      case ElementType::Stroke: {
        auto stroke = static_cast<const Stroke*>(element);
        if (stroke->colorSource) {
          registerColorSource(stroke->colorSource.get());
        }
        break;
      }
      case ElementType::Group: {
        auto group = static_cast<const Group*>(element);
        for (const auto& child : group->elements) {
          collectFromVectorElement(child.get());
        }
        break;
      }
      default:
        break;
    }
  }
};

//==============================================================================
// Forward declarations
//==============================================================================

static void writeColorSource(XMLBuilder& xml, const ColorSource* node, bool writeId);
static void writeVectorElement(XMLBuilder& xml, const Element* node,
                               const ResourceContext& ctx);
static void writeLayerStyle(XMLBuilder& xml, const LayerStyle* node);
static void writeLayerFilter(XMLBuilder& xml, const LayerFilter* node);
static void writeResource(XMLBuilder& xml, const Node* node, const ResourceContext& ctx);
static void writeLayer(XMLBuilder& xml, const Layer* node, const ResourceContext& ctx);

//==============================================================================
// ColorStop and ColorSource writing
//==============================================================================

static void writeColorStops(XMLBuilder& xml, const std::vector<ColorStop>& stops) {
  for (const auto& stop : stops) {
    xml.openElement("ColorStop");
    xml.addRequiredAttribute("offset", stop.offset);
    xml.addRequiredAttribute("color", stop.color.toHexString(stop.color.alpha < 1.0f));
    xml.closeElementSelfClosing();
  }
}

static void writeColorSource(XMLBuilder& xml, const ColorSource* node, bool writeId) {
  switch (node->type()) {
    case ColorSourceType::SolidColor: {
      auto solid = static_cast<const SolidColor*>(node);
      xml.openElement("SolidColor");
      if (writeId && !solid->id.empty()) {
        xml.addAttribute("id", solid->id);
      }
      xml.addAttribute("color", solid->color.toHexString(solid->color.alpha < 1.0f));
      xml.closeElementSelfClosing();
      break;
    }
    case ColorSourceType::LinearGradient: {
      auto grad = static_cast<const LinearGradient*>(node);
      xml.openElement("LinearGradient");
      if (writeId && !grad->id.empty()) {
        xml.addAttribute("id", grad->id);
      }
      if (grad->startPoint.x != 0 || grad->startPoint.y != 0) {
        xml.addAttribute("startPoint", pointToString(grad->startPoint));
      }
      xml.addRequiredAttribute("endPoint", pointToString(grad->endPoint));
      if (!grad->matrix.isIdentity()) {
        xml.addAttribute("matrix", grad->matrix.toString());
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
    case ColorSourceType::RadialGradient: {
      auto grad = static_cast<const RadialGradient*>(node);
      xml.openElement("RadialGradient");
      if (writeId && !grad->id.empty()) {
        xml.addAttribute("id", grad->id);
      }
      if (grad->center.x != 0 || grad->center.y != 0) {
        xml.addAttribute("center", pointToString(grad->center));
      }
      xml.addRequiredAttribute("radius", grad->radius);
      if (!grad->matrix.isIdentity()) {
        xml.addAttribute("matrix", grad->matrix.toString());
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
    case ColorSourceType::ConicGradient: {
      auto grad = static_cast<const ConicGradient*>(node);
      xml.openElement("ConicGradient");
      if (writeId && !grad->id.empty()) {
        xml.addAttribute("id", grad->id);
      }
      if (grad->center.x != 0 || grad->center.y != 0) {
        xml.addAttribute("center", pointToString(grad->center));
      }
      xml.addAttribute("startAngle", grad->startAngle);
      xml.addAttribute("endAngle", grad->endAngle, 360.0f);
      if (!grad->matrix.isIdentity()) {
        xml.addAttribute("matrix", grad->matrix.toString());
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
    case ColorSourceType::DiamondGradient: {
      auto grad = static_cast<const DiamondGradient*>(node);
      xml.openElement("DiamondGradient");
      if (writeId && !grad->id.empty()) {
        xml.addAttribute("id", grad->id);
      }
      if (grad->center.x != 0 || grad->center.y != 0) {
        xml.addAttribute("center", pointToString(grad->center));
      }
      xml.addRequiredAttribute("halfDiagonal", grad->halfDiagonal);
      if (!grad->matrix.isIdentity()) {
        xml.addAttribute("matrix", grad->matrix.toString());
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
    case ColorSourceType::ImagePattern: {
      auto pattern = static_cast<const ImagePattern*>(node);
      xml.openElement("ImagePattern");
      if (writeId && !pattern->id.empty()) {
        xml.addAttribute("id", pattern->id);
      }
      xml.addAttribute("image", pattern->image);
      if (pattern->tileModeX != TileMode::Clamp) {
        xml.addAttribute("tileModeX", TileModeToString(pattern->tileModeX));
      }
      if (pattern->tileModeY != TileMode::Clamp) {
        xml.addAttribute("tileModeY", TileModeToString(pattern->tileModeY));
      }
      if (pattern->sampling != SamplingMode::Linear) {
        xml.addAttribute("sampling", SamplingModeToString(pattern->sampling));
      }
      if (!pattern->matrix.isIdentity()) {
        xml.addAttribute("matrix", pattern->matrix.toString());
      }
      xml.closeElementSelfClosing();
      break;
    }
  }
}

// Write ColorSource with assigned id (for Resources section)
static void writeColorSourceWithId(XMLBuilder& xml, const ColorSource* node,
                                   const std::string& id) {
  switch (node->type()) {
    case ColorSourceType::SolidColor: {
      auto solid = static_cast<const SolidColor*>(node);
      xml.openElement("SolidColor");
      xml.addAttribute("id", id);
      xml.addAttribute("color", solid->color.toHexString(solid->color.alpha < 1.0f));
      xml.closeElementSelfClosing();
      break;
    }
    case ColorSourceType::LinearGradient: {
      auto grad = static_cast<const LinearGradient*>(node);
      xml.openElement("LinearGradient");
      xml.addAttribute("id", id);
      if (grad->startPoint.x != 0 || grad->startPoint.y != 0) {
        xml.addAttribute("startPoint", pointToString(grad->startPoint));
      }
      xml.addRequiredAttribute("endPoint", pointToString(grad->endPoint));
      if (!grad->matrix.isIdentity()) {
        xml.addAttribute("matrix", grad->matrix.toString());
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
    case ColorSourceType::RadialGradient: {
      auto grad = static_cast<const RadialGradient*>(node);
      xml.openElement("RadialGradient");
      xml.addAttribute("id", id);
      if (grad->center.x != 0 || grad->center.y != 0) {
        xml.addAttribute("center", pointToString(grad->center));
      }
      xml.addRequiredAttribute("radius", grad->radius);
      if (!grad->matrix.isIdentity()) {
        xml.addAttribute("matrix", grad->matrix.toString());
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
    case ColorSourceType::ConicGradient: {
      auto grad = static_cast<const ConicGradient*>(node);
      xml.openElement("ConicGradient");
      xml.addAttribute("id", id);
      if (grad->center.x != 0 || grad->center.y != 0) {
        xml.addAttribute("center", pointToString(grad->center));
      }
      xml.addAttribute("startAngle", grad->startAngle);
      xml.addAttribute("endAngle", grad->endAngle, 360.0f);
      if (!grad->matrix.isIdentity()) {
        xml.addAttribute("matrix", grad->matrix.toString());
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
    case ColorSourceType::DiamondGradient: {
      auto grad = static_cast<const DiamondGradient*>(node);
      xml.openElement("DiamondGradient");
      xml.addAttribute("id", id);
      if (grad->center.x != 0 || grad->center.y != 0) {
        xml.addAttribute("center", pointToString(grad->center));
      }
      xml.addRequiredAttribute("halfDiagonal", grad->halfDiagonal);
      if (!grad->matrix.isIdentity()) {
        xml.addAttribute("matrix", grad->matrix.toString());
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
    case ColorSourceType::ImagePattern: {
      auto pattern = static_cast<const ImagePattern*>(node);
      xml.openElement("ImagePattern");
      xml.addAttribute("id", id);
      xml.addAttribute("image", pattern->image);
      if (pattern->tileModeX != TileMode::Clamp) {
        xml.addAttribute("tileModeX", TileModeToString(pattern->tileModeX));
      }
      if (pattern->tileModeY != TileMode::Clamp) {
        xml.addAttribute("tileModeY", TileModeToString(pattern->tileModeY));
      }
      if (pattern->sampling != SamplingMode::Linear) {
        xml.addAttribute("sampling", SamplingModeToString(pattern->sampling));
      }
      if (!pattern->matrix.isIdentity()) {
        xml.addAttribute("matrix", pattern->matrix.toString());
      }
      xml.closeElementSelfClosing();
      break;
    }
  }
}

//==============================================================================
// VectorElement writing
//==============================================================================

static void writeVectorElement(XMLBuilder& xml, const Element* node,
                               const ResourceContext& ctx) {
  switch (node->type()) {
    case ElementType::Rectangle: {
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
    case ElementType::Ellipse: {
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
    case ElementType::Polystar: {
      auto polystar = static_cast<const Polystar*>(node);
      xml.openElement("Polystar");
      if (polystar->center.x != 0 || polystar->center.y != 0) {
        xml.addAttribute("center", pointToString(polystar->center));
      }
      xml.addAttribute("polystarType", PolystarTypeToString(polystar->polystarType));
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
    case ElementType::Path: {
      auto path = static_cast<const Path*>(node);
      xml.openElement("Path");
      if (!path->data.isEmpty()) {
        // Always reference PathData from Resources
        std::string svgStr = path->data.toSVGString();
        auto it = ctx.pathDataMap.find(svgStr);
        if (it != ctx.pathDataMap.end()) {
          xml.addAttribute("data", "#" + it->second);
        } else {
          // Fallback to inline if not found (shouldn't happen)
          xml.addAttribute("data", svgStr);
        }
      }
      xml.addAttribute("reversed", path->reversed);
      xml.closeElementSelfClosing();
      break;
    }
    case ElementType::TextSpan: {
      auto text = static_cast<const TextSpan*>(node);
      xml.openElement("TextSpan");
      xml.addAttribute("x", text->x);
      xml.addAttribute("y", text->y);
      xml.addAttribute("font", text->font);
      xml.addAttribute("fontSize", text->fontSize, 12.0f);
      xml.addAttribute("fontWeight", text->fontWeight, 400);
      if (text->fontStyle != "normal" && !text->fontStyle.empty()) {
        xml.addAttribute("fontStyle", text->fontStyle);
      }
      xml.addAttribute("tracking", text->tracking);
      xml.addAttribute("baselineShift", text->baselineShift);
      xml.closeElementStart();
      xml.addTextContent(text->text);
      xml.closeElement();
      break;
    }
    case ElementType::Fill: {
      auto fill = static_cast<const Fill*>(node);
      xml.openElement("Fill");
      // Check if ColorSource should be referenced or inlined
      if (fill->colorSource) {
        std::string colorId = ctx.getColorSourceId(fill->colorSource.get());
        if (!colorId.empty()) {
          // Reference the ColorSource from Resources
          xml.addAttribute("color", "#" + colorId);
        }
        // If colorId is empty, we'll inline it below
      } else {
        xml.addAttribute("color", fill->color);
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
      // Inline ColorSource only if not extracted to Resources
      if (fill->colorSource && ctx.getColorSourceId(fill->colorSource.get()).empty()) {
        xml.closeElementStart();
        writeColorSource(xml, fill->colorSource.get(), false);
        xml.closeElement();
      } else {
        xml.closeElementSelfClosing();
      }
      break;
    }
    case ElementType::Stroke: {
      auto stroke = static_cast<const Stroke*>(node);
      xml.openElement("Stroke");
      // Check if ColorSource should be referenced or inlined
      if (stroke->colorSource) {
        std::string colorId = ctx.getColorSourceId(stroke->colorSource.get());
        if (!colorId.empty()) {
          // Reference the ColorSource from Resources
          xml.addAttribute("color", "#" + colorId);
        }
        // If colorId is empty, we'll inline it below
      } else {
        xml.addAttribute("color", stroke->color);
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
      // Inline ColorSource only if not extracted to Resources
      if (stroke->colorSource && ctx.getColorSourceId(stroke->colorSource.get()).empty()) {
        xml.closeElementStart();
        writeColorSource(xml, stroke->colorSource.get(), false);
        xml.closeElement();
      } else {
        xml.closeElementSelfClosing();
      }
      break;
    }
    case ElementType::TrimPath: {
      auto trim = static_cast<const TrimPath*>(node);
      xml.openElement("TrimPath");
      xml.addAttribute("start", trim->start);
      xml.addAttribute("end", trim->end, 1.0f);
      xml.addAttribute("offset", trim->offset);
      if (trim->trimType != TrimType::Separate) {
        xml.addAttribute("type", TrimTypeToString(trim->trimType));
      }
      xml.closeElementSelfClosing();
      break;
    }
    case ElementType::RoundCorner: {
      auto round = static_cast<const RoundCorner*>(node);
      xml.openElement("RoundCorner");
      xml.addAttribute("radius", round->radius, 10.0f);
      xml.closeElementSelfClosing();
      break;
    }
    case ElementType::MergePath: {
      auto merge = static_cast<const MergePath*>(node);
      xml.openElement("MergePath");
      if (merge->mode != MergePathMode::Append) {
        xml.addAttribute("mode", MergePathModeToString(merge->mode));
      }
      xml.closeElementSelfClosing();
      break;
    }
    case ElementType::TextModifier: {
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
      xml.addAttribute("fillColor", modifier->fillColor);
      xml.addAttribute("strokeColor", modifier->strokeColor);
      if (modifier->strokeWidth >= 0) {
        xml.addAttribute("strokeWidth", modifier->strokeWidth);
      }
      if (modifier->selectors.empty()) {
        xml.closeElementSelfClosing();
      } else {
        xml.closeElementStart();
        for (const auto& selector : modifier->selectors) {
          if (selector->type() != TextSelectorType::RangeSelector) {
            continue;
          }
          auto rangeSelector = static_cast<const RangeSelector*>(selector.get());
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
          xml.addAttribute("randomizeOrder", rangeSelector->randomizeOrder);
          xml.addAttribute("randomSeed", rangeSelector->randomSeed);
          xml.closeElementSelfClosing();
        }
        xml.closeElement();
      }
      break;
    }
    case ElementType::TextPath: {
      auto textPath = static_cast<const TextPath*>(node);
      xml.openElement("TextPath");
      xml.addAttribute("path", textPath->path);
      if (textPath->pathAlign != TextPathAlign::Start) {
        xml.addAttribute("align", TextPathAlignToString(textPath->pathAlign));
      }
      xml.addAttribute("firstMargin", textPath->firstMargin);
      xml.addAttribute("lastMargin", textPath->lastMargin);
      xml.addAttribute("perpendicularToPath", textPath->perpendicularToPath, true);
      xml.addAttribute("reversed", textPath->reversed);
      xml.addAttribute("forceAlignment", textPath->forceAlignment);
      xml.closeElementSelfClosing();
      break;
    }
    case ElementType::TextLayout: {
      auto layout = static_cast<const TextLayout*>(node);
      xml.openElement("TextLayout");
      xml.addRequiredAttribute("width", layout->width);
      xml.addRequiredAttribute("height", layout->height);
      if (layout->textAlign != TextAlign::Left) {
        xml.addAttribute("align", TextAlignToString(layout->textAlign));
      }
      if (layout->verticalAlign != VerticalAlign::Top) {
        xml.addAttribute("verticalAlign", VerticalAlignToString(layout->verticalAlign));
      }
      xml.addAttribute("lineHeight", layout->lineHeight, 1.2f);
      xml.addAttribute("indent", layout->indent);
      if (layout->overflow != Overflow::Clip) {
        xml.addAttribute("overflow", OverflowToString(layout->overflow));
      }
      xml.closeElementSelfClosing();
      break;
    }
    case ElementType::Repeater: {
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
    case ElementType::Group: {
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
          writeVectorElement(xml, element.get(), ctx);
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
  switch (node->type()) {
    case LayerStyleType::DropShadowStyle: {
      auto style = static_cast<const DropShadowStyle*>(node);
      xml.openElement("DropShadowStyle");
      if (style->blendMode != BlendMode::Normal) {
        xml.addAttribute("blendMode", BlendModeToString(style->blendMode));
      }
      xml.addAttribute("offsetX", style->offsetX);
      xml.addAttribute("offsetY", style->offsetY);
      xml.addAttribute("blurrinessX", style->blurrinessX);
      xml.addAttribute("blurrinessY", style->blurrinessY);
      xml.addAttribute("color", style->color.toHexString(style->color.alpha < 1.0f));
      xml.addAttribute("showBehindLayer", style->showBehindLayer, true);
      xml.closeElementSelfClosing();
      break;
    }
    case LayerStyleType::InnerShadowStyle: {
      auto style = static_cast<const InnerShadowStyle*>(node);
      xml.openElement("InnerShadowStyle");
      if (style->blendMode != BlendMode::Normal) {
        xml.addAttribute("blendMode", BlendModeToString(style->blendMode));
      }
      xml.addAttribute("offsetX", style->offsetX);
      xml.addAttribute("offsetY", style->offsetY);
      xml.addAttribute("blurrinessX", style->blurrinessX);
      xml.addAttribute("blurrinessY", style->blurrinessY);
      xml.addAttribute("color", style->color.toHexString(style->color.alpha < 1.0f));
      xml.closeElementSelfClosing();
      break;
    }
    case LayerStyleType::BackgroundBlurStyle: {
      auto style = static_cast<const BackgroundBlurStyle*>(node);
      xml.openElement("BackgroundBlurStyle");
      if (style->blendMode != BlendMode::Normal) {
        xml.addAttribute("blendMode", BlendModeToString(style->blendMode));
      }
      xml.addAttribute("blurrinessX", style->blurrinessX);
      xml.addAttribute("blurrinessY", style->blurrinessY);
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
  switch (node->type()) {
    case LayerFilterType::BlurFilter: {
      auto filter = static_cast<const BlurFilter*>(node);
      xml.openElement("BlurFilter");
      xml.addRequiredAttribute("blurrinessX", filter->blurrinessX);
      xml.addRequiredAttribute("blurrinessY", filter->blurrinessY);
      if (filter->tileMode != TileMode::Decal) {
        xml.addAttribute("tileMode", TileModeToString(filter->tileMode));
      }
      xml.closeElementSelfClosing();
      break;
    }
    case LayerFilterType::DropShadowFilter: {
      auto filter = static_cast<const DropShadowFilter*>(node);
      xml.openElement("DropShadowFilter");
      xml.addAttribute("offsetX", filter->offsetX);
      xml.addAttribute("offsetY", filter->offsetY);
      xml.addAttribute("blurrinessX", filter->blurrinessX);
      xml.addAttribute("blurrinessY", filter->blurrinessY);
      xml.addAttribute("color", filter->color.toHexString(filter->color.alpha < 1.0f));
      xml.addAttribute("shadowOnly", filter->shadowOnly);
      xml.closeElementSelfClosing();
      break;
    }
    case LayerFilterType::InnerShadowFilter: {
      auto filter = static_cast<const InnerShadowFilter*>(node);
      xml.openElement("InnerShadowFilter");
      xml.addAttribute("offsetX", filter->offsetX);
      xml.addAttribute("offsetY", filter->offsetY);
      xml.addAttribute("blurrinessX", filter->blurrinessX);
      xml.addAttribute("blurrinessY", filter->blurrinessY);
      xml.addAttribute("color", filter->color.toHexString(filter->color.alpha < 1.0f));
      xml.addAttribute("shadowOnly", filter->shadowOnly);
      xml.closeElementSelfClosing();
      break;
    }
    case LayerFilterType::BlendFilter: {
      auto filter = static_cast<const BlendFilter*>(node);
      xml.openElement("BlendFilter");
      xml.addAttribute("color", filter->color.toHexString(filter->color.alpha < 1.0f));
      if (filter->blendMode != BlendMode::Normal) {
        xml.addAttribute("blendMode", BlendModeToString(filter->blendMode));
      }
      xml.closeElementSelfClosing();
      break;
    }
    case LayerFilterType::ColorMatrixFilter: {
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

static void writeResource(XMLBuilder& xml, const Node* node, const ResourceContext& ctx) {
  switch (node->nodeType()) {
    case NodeType::Image: {
      auto image = static_cast<const Image*>(node);
      xml.openElement("Image");
      xml.addAttribute("id", image->id);
      xml.addAttribute("source", image->source);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::PathData: {
      auto pathData = static_cast<const PathDataResource*>(node);
      xml.openElement("PathData");
      xml.addAttribute("id", pathData->id);
      xml.addAttribute("data", pathData->data);
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
          writeLayer(xml, layer.get(), ctx);
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
// Layer writing
//==============================================================================

static void writeLayer(XMLBuilder& xml, const Layer* node, const ResourceContext& ctx) {
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
    xml.addAttribute("matrix", node->matrix.toString());
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
  xml.addAttribute("mask", node->mask);
  if (node->maskType != MaskType::Alpha) {
    xml.addAttribute("maskType", MaskTypeToString(node->maskType));
  }
  xml.addAttribute("composition", node->composition);

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

  if (!node->contents.empty()) {
    xml.openElement("contents");
    xml.closeElementStart();
    for (const auto& element : node->contents) {
      writeVectorElement(xml, element.get(), ctx);
    }
    xml.closeElement();
  }

  if (!node->styles.empty()) {
    xml.openElement("styles");
    xml.closeElementStart();
    for (const auto& style : node->styles) {
      writeLayerStyle(xml, style.get());
    }
    xml.closeElement();
  }

  if (!node->filters.empty()) {
    xml.openElement("filters");
    xml.closeElementStart();
    for (const auto& filter : node->filters) {
      writeLayerFilter(xml, filter.get());
    }
    xml.closeElement();
  }

  for (const auto& child : node->children) {
    writeLayer(xml, child.get(), ctx);
  }

  xml.closeElement();
}

//==============================================================================
// Main Write function
//==============================================================================

std::string PAGXXMLWriter::Write(const Document& doc) {
  // First pass: collect resources and count references
  ResourceContext ctx = {};
  ctx.collectFromDocument(doc);
  ctx.finalizeColorSources();

  // Build ColorSource resources list (only those with multiple references)
  // We need to store pointers to actual ColorSource nodes for writing
  std::unordered_map<std::string, const ColorSource*> colorSourceByKey = {};
  for (const auto& layer : doc.layers) {
    std::function<void(const Layer*)> collectColorSources = [&](const Layer* layer) {
      for (const auto& element : layer->contents) {
        if (element->type() == ElementType::Fill) {
          auto fill = static_cast<const Fill*>(element.get());
          if (fill->colorSource && ctx.shouldExtractColorSource(fill->colorSource.get())) {
            std::string key = colorSourceToKey(fill->colorSource.get());
            if (colorSourceByKey.find(key) == colorSourceByKey.end()) {
              colorSourceByKey[key] = fill->colorSource.get();
            }
          }
        } else if (element->type() == ElementType::Stroke) {
          auto stroke = static_cast<const Stroke*>(element.get());
          if (stroke->colorSource && ctx.shouldExtractColorSource(stroke->colorSource.get())) {
            std::string key = colorSourceToKey(stroke->colorSource.get());
            if (colorSourceByKey.find(key) == colorSourceByKey.end()) {
              colorSourceByKey[key] = stroke->colorSource.get();
            }
          }
        } else if (element->type() == ElementType::Group) {
          auto group = static_cast<const Group*>(element.get());
          std::function<void(const Group*)> collectFromGroup = [&](const Group* g) {
            for (const auto& child : g->elements) {
              if (child->type() == ElementType::Fill) {
                auto fill = static_cast<const Fill*>(child.get());
                if (fill->colorSource && ctx.shouldExtractColorSource(fill->colorSource.get())) {
                  std::string key = colorSourceToKey(fill->colorSource.get());
                  if (colorSourceByKey.find(key) == colorSourceByKey.end()) {
                    colorSourceByKey[key] = fill->colorSource.get();
                  }
                }
              } else if (child->type() == ElementType::Stroke) {
                auto stroke = static_cast<const Stroke*>(child.get());
                if (stroke->colorSource && ctx.shouldExtractColorSource(stroke->colorSource.get())) {
                  std::string key = colorSourceToKey(stroke->colorSource.get());
                  if (colorSourceByKey.find(key) == colorSourceByKey.end()) {
                    colorSourceByKey[key] = stroke->colorSource.get();
                  }
                }
              } else if (child->type() == ElementType::Group) {
                collectFromGroup(static_cast<const Group*>(child.get()));
              }
            }
          };
          collectFromGroup(group);
        }
      }
      for (const auto& child : layer->children) {
        collectColorSources(child.get());
      }
    };
    collectColorSources(layer.get());
  }

  // Also collect from Compositions
  for (const auto& resource : doc.resources) {
    if (resource->nodeType() == NodeType::Composition) {
      auto comp = static_cast<const Composition*>(resource.get());
      std::function<void(const Layer*)> collectColorSources = [&](const Layer* layer) {
        for (const auto& element : layer->contents) {
          if (element->type() == ElementType::Fill) {
            auto fill = static_cast<const Fill*>(element.get());
            if (fill->colorSource && ctx.shouldExtractColorSource(fill->colorSource.get())) {
              std::string key = colorSourceToKey(fill->colorSource.get());
              if (colorSourceByKey.find(key) == colorSourceByKey.end()) {
                colorSourceByKey[key] = fill->colorSource.get();
              }
            }
          } else if (element->type() == ElementType::Stroke) {
            auto stroke = static_cast<const Stroke*>(element.get());
            if (stroke->colorSource && ctx.shouldExtractColorSource(stroke->colorSource.get())) {
              std::string key = colorSourceToKey(stroke->colorSource.get());
              if (colorSourceByKey.find(key) == colorSourceByKey.end()) {
                colorSourceByKey[key] = stroke->colorSource.get();
              }
            }
          } else if (element->type() == ElementType::Group) {
            auto group = static_cast<const Group*>(element.get());
            std::function<void(const Group*)> collectFromGroup = [&](const Group* g) {
              for (const auto& child : g->elements) {
                if (child->type() == ElementType::Fill) {
                  auto fill = static_cast<const Fill*>(child.get());
                  if (fill->colorSource && ctx.shouldExtractColorSource(fill->colorSource.get())) {
                    std::string key = colorSourceToKey(fill->colorSource.get());
                    if (colorSourceByKey.find(key) == colorSourceByKey.end()) {
                      colorSourceByKey[key] = fill->colorSource.get();
                    }
                  }
                } else if (child->type() == ElementType::Stroke) {
                  auto stroke = static_cast<const Stroke*>(child.get());
                  if (stroke->colorSource &&
                      ctx.shouldExtractColorSource(stroke->colorSource.get())) {
                    std::string key = colorSourceToKey(stroke->colorSource.get());
                    if (colorSourceByKey.find(key) == colorSourceByKey.end()) {
                      colorSourceByKey[key] = stroke->colorSource.get();
                    }
                  }
                } else if (child->type() == ElementType::Group) {
                  collectFromGroup(static_cast<const Group*>(child.get()));
                }
              }
            };
            collectFromGroup(group);
          }
        }
        for (const auto& child : layer->children) {
          collectColorSources(child.get());
        }
      };
      for (const auto& layer : comp->layers) {
        collectColorSources(layer.get());
      }
    }
  }

  // Second pass: generate XML
  XMLBuilder xml = {};
  xml.appendDeclaration();

  xml.openElement("pagx");
  xml.addAttribute("version", doc.version);
  xml.addAttribute("width", doc.width);
  xml.addAttribute("height", doc.height);
  xml.closeElementStart();

  // Write Layers first (for better readability)
  for (const auto& layer : doc.layers) {
    writeLayer(xml, layer.get(), ctx);
  }

  // Write Resources section at the end
  bool hasResources = !ctx.pathDataResources.empty() || !colorSourceByKey.empty() ||
                      !doc.resources.empty();
  if (hasResources) {
    xml.openElement("Resources");
    xml.closeElementStart();

    // Write PathData resources
    for (const auto& [id, svgString] : ctx.pathDataResources) {
      xml.openElement("PathData");
      xml.addAttribute("id", id);
      xml.addAttribute("data", svgString);
      xml.closeElementSelfClosing();
    }

    // Write ColorSource resources (those with multiple references)
    for (const auto& [key, node] : colorSourceByKey) {
      std::string id = ctx.getColorSourceId(node);
      if (!id.empty()) {
        writeColorSourceWithId(xml, node, id);
      }
    }

    // Write original resources (Image, Composition, etc.)
    for (const auto& resource : doc.resources) {
      writeResource(xml, resource.get(), ctx);
    }

    xml.closeElement();
  }

  xml.closeElement();

  return xml.str();
}

}  // namespace pagx
