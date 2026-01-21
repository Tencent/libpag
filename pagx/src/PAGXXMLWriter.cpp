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

static std::string colorSourceToKey(const ColorSourceNode* node) {
  if (!node) {
    return "";
  }
  std::ostringstream oss = {};
  switch (node->type()) {
    case NodeType::SolidColor: {
      auto solid = static_cast<const SolidColorNode*>(node);
      oss << "SolidColor:" << solid->color.toHexString(true);
      break;
    }
    case NodeType::LinearGradient: {
      auto grad = static_cast<const LinearGradientNode*>(node);
      oss << "LinearGradient:" << grad->startPoint.x << "," << grad->startPoint.y << ":"
          << grad->endPoint.x << "," << grad->endPoint.y << ":" << grad->matrix.toString() << ":";
      for (const auto& stop : grad->colorStops) {
        oss << stop.offset << "=" << stop.color.toHexString(true) << ";";
      }
      break;
    }
    case NodeType::RadialGradient: {
      auto grad = static_cast<const RadialGradientNode*>(node);
      oss << "RadialGradient:" << grad->center.x << "," << grad->center.y << ":" << grad->radius
          << ":" << grad->matrix.toString() << ":";
      for (const auto& stop : grad->colorStops) {
        oss << stop.offset << "=" << stop.color.toHexString(true) << ";";
      }
      break;
    }
    case NodeType::ConicGradient: {
      auto grad = static_cast<const ConicGradientNode*>(node);
      oss << "ConicGradient:" << grad->center.x << "," << grad->center.y << ":" << grad->startAngle
          << ":" << grad->endAngle << ":" << grad->matrix.toString() << ":";
      for (const auto& stop : grad->colorStops) {
        oss << stop.offset << "=" << stop.color.toHexString(true) << ";";
      }
      break;
    }
    case NodeType::DiamondGradient: {
      auto grad = static_cast<const DiamondGradientNode*>(node);
      oss << "DiamondGradient:" << grad->center.x << "," << grad->center.y << ":"
          << grad->halfDiagonal << ":" << grad->matrix.toString() << ":";
      for (const auto& stop : grad->colorStops) {
        oss << stop.offset << "=" << stop.color.toHexString(true) << ";";
      }
      break;
    }
    case NodeType::ImagePattern: {
      auto pattern = static_cast<const ImagePatternNode*>(node);
      oss << "ImagePattern:" << pattern->image << ":" << static_cast<int>(pattern->tileModeX) << ":"
          << static_cast<int>(pattern->tileModeY) << ":" << static_cast<int>(pattern->sampling)
          << ":" << pattern->matrix.toString();
      break;
    }
    default:
      break;
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
  std::vector<std::pair<std::string, const ColorSourceNode*>> colorSourceResources = {};

  int nextPathId = 1;
  int nextColorId = 1;

  // First pass: collect and count all resources
  void collectFromDocument(const PAGXDocument& doc) {
    for (const auto& layer : doc.layers) {
      collectFromLayer(layer.get());
    }
    for (const auto& resource : doc.resources) {
      if (resource->type() == NodeType::Composition) {
        auto comp = static_cast<const CompositionNode*>(resource.get());
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
  void registerColorSource(const ColorSourceNode* node) {
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
  bool shouldExtractColorSource(const ColorSourceNode* node) const {
    if (!node) {
      return false;
    }
    std::string key = colorSourceToKey(node);
    auto it = colorSourceMap.find(key);
    return it != colorSourceMap.end() && it->second.second > 1;
  }

  // Get ColorSource resource id (empty if should inline)
  std::string getColorSourceId(const ColorSourceNode* node) const {
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
  void collectFromLayer(const LayerNode* layer) {
    for (const auto& element : layer->contents) {
      collectFromVectorElement(element.get());
    }
    for (const auto& child : layer->children) {
      collectFromLayer(child.get());
    }
  }

  void collectFromVectorElement(const VectorElementNode* element) {
    switch (element->type()) {
      case NodeType::Path: {
        auto path = static_cast<const PathNode*>(element);
        if (!path->data.isEmpty()) {
          getPathDataId(path->data.toSVGString());
        }
        break;
      }
      case NodeType::Fill: {
        auto fill = static_cast<const FillNode*>(element);
        if (fill->colorSource) {
          registerColorSource(fill->colorSource.get());
        }
        break;
      }
      case NodeType::Stroke: {
        auto stroke = static_cast<const StrokeNode*>(element);
        if (stroke->colorSource) {
          registerColorSource(stroke->colorSource.get());
        }
        break;
      }
      case NodeType::Group: {
        auto group = static_cast<const GroupNode*>(element);
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

static void writeColorSource(XMLBuilder& xml, const ColorSourceNode* node, bool writeId);
static void writeVectorElement(XMLBuilder& xml, const VectorElementNode* node,
                               const ResourceContext& ctx);
static void writeLayerStyle(XMLBuilder& xml, const LayerStyleNode* node);
static void writeLayerFilter(XMLBuilder& xml, const LayerFilterNode* node);
static void writeResource(XMLBuilder& xml, const ResourceNode* node, const ResourceContext& ctx);
static void writeLayer(XMLBuilder& xml, const LayerNode* node, const ResourceContext& ctx);

//==============================================================================
// ColorStop and ColorSource writing
//==============================================================================

static void writeColorStops(XMLBuilder& xml, const std::vector<ColorStopNode>& stops) {
  for (const auto& stop : stops) {
    xml.openElement("ColorStop");
    xml.addRequiredAttribute("offset", stop.offset);
    xml.addRequiredAttribute("color", stop.color.toHexString(stop.color.alpha < 1.0f));
    xml.closeElementSelfClosing();
  }
}

static void writeColorSource(XMLBuilder& xml, const ColorSourceNode* node, bool writeId) {
  switch (node->type()) {
    case NodeType::SolidColor: {
      auto solid = static_cast<const SolidColorNode*>(node);
      xml.openElement("SolidColor");
      if (writeId && !solid->id.empty()) {
        xml.addAttribute("id", solid->id);
      }
      xml.addAttribute("color", solid->color.toHexString(solid->color.alpha < 1.0f));
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::LinearGradient: {
      auto grad = static_cast<const LinearGradientNode*>(node);
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
    case NodeType::RadialGradient: {
      auto grad = static_cast<const RadialGradientNode*>(node);
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
    case NodeType::ConicGradient: {
      auto grad = static_cast<const ConicGradientNode*>(node);
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
    case NodeType::DiamondGradient: {
      auto grad = static_cast<const DiamondGradientNode*>(node);
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
    case NodeType::ImagePattern: {
      auto pattern = static_cast<const ImagePatternNode*>(node);
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
    default:
      break;
  }
}

// Write ColorSource with assigned id (for Resources section)
static void writeColorSourceWithId(XMLBuilder& xml, const ColorSourceNode* node,
                                   const std::string& id) {
  switch (node->type()) {
    case NodeType::SolidColor: {
      auto solid = static_cast<const SolidColorNode*>(node);
      xml.openElement("SolidColor");
      xml.addAttribute("id", id);
      xml.addAttribute("color", solid->color.toHexString(solid->color.alpha < 1.0f));
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::LinearGradient: {
      auto grad = static_cast<const LinearGradientNode*>(node);
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
    case NodeType::RadialGradient: {
      auto grad = static_cast<const RadialGradientNode*>(node);
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
    case NodeType::ConicGradient: {
      auto grad = static_cast<const ConicGradientNode*>(node);
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
    case NodeType::DiamondGradient: {
      auto grad = static_cast<const DiamondGradientNode*>(node);
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
    case NodeType::ImagePattern: {
      auto pattern = static_cast<const ImagePatternNode*>(node);
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
    default:
      break;
  }
}

//==============================================================================
// VectorElement writing
//==============================================================================

static void writeVectorElement(XMLBuilder& xml, const VectorElementNode* node,
                               const ResourceContext& ctx) {
  switch (node->type()) {
    case NodeType::Rectangle: {
      auto rect = static_cast<const RectangleNode*>(node);
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
      auto ellipse = static_cast<const EllipseNode*>(node);
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
      auto polystar = static_cast<const PolystarNode*>(node);
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
    case NodeType::Path: {
      auto path = static_cast<const PathNode*>(node);
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
    case NodeType::TextSpan: {
      auto text = static_cast<const TextSpanNode*>(node);
      xml.openElement("TextSpan");
      xml.addAttribute("x", text->x);
      xml.addAttribute("y", text->y);
      xml.addAttribute("font", text->font);
      xml.addAttribute("fontSize", text->fontSize, 12.0f);
      xml.addAttribute("fontWeight", text->fontWeight, 400);
      if (text->fontStyle != FontStyle::Normal) {
        xml.addAttribute("fontStyle", FontStyleToString(text->fontStyle));
      }
      xml.addAttribute("tracking", text->tracking);
      xml.addAttribute("baselineShift", text->baselineShift);
      if (text->textAnchor != TextAnchor::Start) {
        xml.addAttribute("textAnchor", TextAnchorToString(text->textAnchor));
      }
      xml.closeElementStart();
      xml.addTextContent(text->text);
      xml.closeElement();
      break;
    }
    case NodeType::Fill: {
      auto fill = static_cast<const FillNode*>(node);
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
      if (fill->placement != Placement::Background) {
        xml.addAttribute("placement", PlacementToString(fill->placement));
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
    case NodeType::Stroke: {
      auto stroke = static_cast<const StrokeNode*>(node);
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
      xml.addAttribute("width", stroke->strokeWidth, 1.0f);
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
      if (stroke->placement != Placement::Background) {
        xml.addAttribute("placement", PlacementToString(stroke->placement));
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
    case NodeType::TrimPath: {
      auto trim = static_cast<const TrimPathNode*>(node);
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
    case NodeType::RoundCorner: {
      auto round = static_cast<const RoundCornerNode*>(node);
      xml.openElement("RoundCorner");
      xml.addAttribute("radius", round->radius, 10.0f);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::MergePath: {
      auto merge = static_cast<const MergePathNode*>(node);
      xml.openElement("MergePath");
      if (merge->mode != MergePathMode::Append) {
        xml.addAttribute("mode", MergePathModeToString(merge->mode));
      }
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::TextModifier: {
      auto modifier = static_cast<const TextModifierNode*>(node);
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
      if (modifier->rangeSelectors.empty()) {
        xml.closeElementSelfClosing();
      } else {
        xml.closeElementStart();
        for (const auto& selector : modifier->rangeSelectors) {
          xml.openElement("RangeSelector");
          xml.addAttribute("start", selector.start);
          xml.addAttribute("end", selector.end, 1.0f);
          xml.addAttribute("offset", selector.offset);
          if (selector.unit != SelectorUnit::Percentage) {
            xml.addAttribute("unit", SelectorUnitToString(selector.unit));
          }
          if (selector.shape != SelectorShape::Square) {
            xml.addAttribute("shape", SelectorShapeToString(selector.shape));
          }
          xml.addAttribute("easeIn", selector.easeIn);
          xml.addAttribute("easeOut", selector.easeOut);
          if (selector.mode != SelectorMode::Add) {
            xml.addAttribute("mode", SelectorModeToString(selector.mode));
          }
          xml.addAttribute("weight", selector.weight, 1.0f);
          xml.addAttribute("randomizeOrder", selector.randomizeOrder);
          xml.addAttribute("randomSeed", selector.randomSeed);
          xml.closeElementSelfClosing();
        }
        xml.closeElement();
      }
      break;
    }
    case NodeType::TextPath: {
      auto textPath = static_cast<const TextPathNode*>(node);
      xml.openElement("TextPath");
      xml.addAttribute("path", textPath->path);
      if (textPath->textPathAlign != TextPathAlign::Start) {
        xml.addAttribute("align", TextPathAlignToString(textPath->textPathAlign));
      }
      xml.addAttribute("firstMargin", textPath->firstMargin);
      xml.addAttribute("lastMargin", textPath->lastMargin);
      xml.addAttribute("perpendicularToPath", textPath->perpendicularToPath, true);
      xml.addAttribute("reversed", textPath->reversed);
      xml.addAttribute("forceAlignment", textPath->forceAlignment);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::TextLayout: {
      auto layout = static_cast<const TextLayoutNode*>(node);
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
    case NodeType::Repeater: {
      auto repeater = static_cast<const RepeaterNode*>(node);
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
      auto group = static_cast<const GroupNode*>(node);
      xml.openElement("Group");
      xml.addAttribute("name", group->name);
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

static void writeLayerStyle(XMLBuilder& xml, const LayerStyleNode* node) {
  switch (node->type()) {
    case NodeType::DropShadowStyle: {
      auto style = static_cast<const DropShadowStyleNode*>(node);
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
    case NodeType::InnerShadowStyle: {
      auto style = static_cast<const InnerShadowStyleNode*>(node);
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
    case NodeType::BackgroundBlurStyle: {
      auto style = static_cast<const BackgroundBlurStyleNode*>(node);
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

static void writeLayerFilter(XMLBuilder& xml, const LayerFilterNode* node) {
  switch (node->type()) {
    case NodeType::BlurFilter: {
      auto filter = static_cast<const BlurFilterNode*>(node);
      xml.openElement("BlurFilter");
      xml.addRequiredAttribute("blurrinessX", filter->blurrinessX);
      xml.addRequiredAttribute("blurrinessY", filter->blurrinessY);
      if (filter->tileMode != TileMode::Decal) {
        xml.addAttribute("tileMode", TileModeToString(filter->tileMode));
      }
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::DropShadowFilter: {
      auto filter = static_cast<const DropShadowFilterNode*>(node);
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
    case NodeType::InnerShadowFilter: {
      auto filter = static_cast<const InnerShadowFilterNode*>(node);
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
    case NodeType::BlendFilter: {
      auto filter = static_cast<const BlendFilterNode*>(node);
      xml.openElement("BlendFilter");
      xml.addAttribute("color", filter->color.toHexString(filter->color.alpha < 1.0f));
      if (filter->filterBlendMode != BlendMode::Normal) {
        xml.addAttribute("blendMode", BlendModeToString(filter->filterBlendMode));
      }
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::ColorMatrixFilter: {
      auto filter = static_cast<const ColorMatrixFilterNode*>(node);
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

static void writeResource(XMLBuilder& xml, const ResourceNode* node, const ResourceContext& ctx) {
  switch (node->type()) {
    case NodeType::Image: {
      auto image = static_cast<const ImageNode*>(node);
      xml.openElement("Image");
      xml.addAttribute("id", image->id);
      xml.addAttribute("source", image->source);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::PathData: {
      auto pathData = static_cast<const PathDataNode*>(node);
      xml.openElement("PathData");
      xml.addAttribute("id", pathData->id);
      xml.addAttribute("data", pathData->data);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::Composition: {
      auto comp = static_cast<const CompositionNode*>(node);
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
    case NodeType::SolidColor:
    case NodeType::LinearGradient:
    case NodeType::RadialGradient:
    case NodeType::ConicGradient:
    case NodeType::DiamondGradient:
    case NodeType::ImagePattern:
      writeColorSource(xml, static_cast<const ColorSourceNode*>(node), true);
      break;
    default:
      break;
  }
}

//==============================================================================
// Layer writing
//==============================================================================

static void writeLayer(XMLBuilder& xml, const LayerNode* node, const ResourceContext& ctx) {
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

std::string PAGXXMLWriter::Write(const PAGXDocument& doc) {
  // First pass: collect resources and count references
  ResourceContext ctx = {};
  ctx.collectFromDocument(doc);
  ctx.finalizeColorSources();

  // Build ColorSource resources list (only those with multiple references)
  // We need to store pointers to actual ColorSource nodes for writing
  std::unordered_map<std::string, const ColorSourceNode*> colorSourceByKey = {};
  for (const auto& layer : doc.layers) {
    std::function<void(const LayerNode*)> collectColorSources = [&](const LayerNode* layer) {
      for (const auto& element : layer->contents) {
        if (element->type() == NodeType::Fill) {
          auto fill = static_cast<const FillNode*>(element.get());
          if (fill->colorSource && ctx.shouldExtractColorSource(fill->colorSource.get())) {
            std::string key = colorSourceToKey(fill->colorSource.get());
            if (colorSourceByKey.find(key) == colorSourceByKey.end()) {
              colorSourceByKey[key] = fill->colorSource.get();
            }
          }
        } else if (element->type() == NodeType::Stroke) {
          auto stroke = static_cast<const StrokeNode*>(element.get());
          if (stroke->colorSource && ctx.shouldExtractColorSource(stroke->colorSource.get())) {
            std::string key = colorSourceToKey(stroke->colorSource.get());
            if (colorSourceByKey.find(key) == colorSourceByKey.end()) {
              colorSourceByKey[key] = stroke->colorSource.get();
            }
          }
        } else if (element->type() == NodeType::Group) {
          auto group = static_cast<const GroupNode*>(element.get());
          std::function<void(const GroupNode*)> collectFromGroup = [&](const GroupNode* g) {
            for (const auto& child : g->elements) {
              if (child->type() == NodeType::Fill) {
                auto fill = static_cast<const FillNode*>(child.get());
                if (fill->colorSource && ctx.shouldExtractColorSource(fill->colorSource.get())) {
                  std::string key = colorSourceToKey(fill->colorSource.get());
                  if (colorSourceByKey.find(key) == colorSourceByKey.end()) {
                    colorSourceByKey[key] = fill->colorSource.get();
                  }
                }
              } else if (child->type() == NodeType::Stroke) {
                auto stroke = static_cast<const StrokeNode*>(child.get());
                if (stroke->colorSource && ctx.shouldExtractColorSource(stroke->colorSource.get())) {
                  std::string key = colorSourceToKey(stroke->colorSource.get());
                  if (colorSourceByKey.find(key) == colorSourceByKey.end()) {
                    colorSourceByKey[key] = stroke->colorSource.get();
                  }
                }
              } else if (child->type() == NodeType::Group) {
                collectFromGroup(static_cast<const GroupNode*>(child.get()));
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
    if (resource->type() == NodeType::Composition) {
      auto comp = static_cast<const CompositionNode*>(resource.get());
      std::function<void(const LayerNode*)> collectColorSources = [&](const LayerNode* layer) {
        for (const auto& element : layer->contents) {
          if (element->type() == NodeType::Fill) {
            auto fill = static_cast<const FillNode*>(element.get());
            if (fill->colorSource && ctx.shouldExtractColorSource(fill->colorSource.get())) {
              std::string key = colorSourceToKey(fill->colorSource.get());
              if (colorSourceByKey.find(key) == colorSourceByKey.end()) {
                colorSourceByKey[key] = fill->colorSource.get();
              }
            }
          } else if (element->type() == NodeType::Stroke) {
            auto stroke = static_cast<const StrokeNode*>(element.get());
            if (stroke->colorSource && ctx.shouldExtractColorSource(stroke->colorSource.get())) {
              std::string key = colorSourceToKey(stroke->colorSource.get());
              if (colorSourceByKey.find(key) == colorSourceByKey.end()) {
                colorSourceByKey[key] = stroke->colorSource.get();
              }
            }
          } else if (element->type() == NodeType::Group) {
            auto group = static_cast<const GroupNode*>(element.get());
            std::function<void(const GroupNode*)> collectFromGroup = [&](const GroupNode* g) {
              for (const auto& child : g->elements) {
                if (child->type() == NodeType::Fill) {
                  auto fill = static_cast<const FillNode*>(child.get());
                  if (fill->colorSource && ctx.shouldExtractColorSource(fill->colorSource.get())) {
                    std::string key = colorSourceToKey(fill->colorSource.get());
                    if (colorSourceByKey.find(key) == colorSourceByKey.end()) {
                      colorSourceByKey[key] = fill->colorSource.get();
                    }
                  }
                } else if (child->type() == NodeType::Stroke) {
                  auto stroke = static_cast<const StrokeNode*>(child.get());
                  if (stroke->colorSource &&
                      ctx.shouldExtractColorSource(stroke->colorSource.get())) {
                    std::string key = colorSourceToKey(stroke->colorSource.get());
                    if (colorSourceByKey.find(key) == colorSourceByKey.end()) {
                      colorSourceByKey[key] = stroke->colorSource.get();
                    }
                  }
                } else if (child->type() == NodeType::Group) {
                  collectFromGroup(static_cast<const GroupNode*>(child.get()));
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
