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

namespace pagx {

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

static void writeColorSource(XMLBuilder& xml, const ColorSourceNode* node);
static void writeVectorElement(XMLBuilder& xml, const VectorElementNode* node);
static void writeLayerStyle(XMLBuilder& xml, const LayerStyleNode* node);
static void writeLayerFilter(XMLBuilder& xml, const LayerFilterNode* node);
static void writeResource(XMLBuilder& xml, const ResourceNode* node);
static void writeLayer(XMLBuilder& xml, const LayerNode* node);

static std::string pointToString(const Point& p) {
  std::ostringstream oss = {};
  oss << p.x << "," << p.y;
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

static void writeColorStops(XMLBuilder& xml, const std::vector<ColorStopNode>& stops) {
  for (const auto& stop : stops) {
    xml.openElement("ColorStop");
    xml.addAttribute("offset", stop.offset);
    xml.addAttribute("color", stop.color.toHexString(stop.color.alpha < 1.0f));
    xml.closeElementSelfClosing();
  }
}

static void writeColorSource(XMLBuilder& xml, const ColorSourceNode* node) {
  switch (node->type()) {
    case NodeType::SolidColor: {
      auto solid = static_cast<const SolidColorNode*>(node);
      xml.openElement("SolidColor");
      xml.addAttribute("id", solid->id);
      xml.addAttribute("color", solid->color.toHexString(solid->color.alpha < 1.0f));
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::LinearGradient: {
      auto grad = static_cast<const LinearGradientNode*>(node);
      xml.openElement("LinearGradient");
      xml.addAttribute("id", grad->id);
      xml.addAttribute("startX", grad->startX);
      xml.addAttribute("startY", grad->startY);
      xml.addAttribute("endX", grad->endX);
      xml.addAttribute("endY", grad->endY);
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
      xml.addAttribute("id", grad->id);
      xml.addAttribute("centerX", grad->centerX);
      xml.addAttribute("centerY", grad->centerY);
      xml.addAttribute("radius", grad->radius);
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
      xml.addAttribute("id", grad->id);
      xml.addAttribute("centerX", grad->centerX);
      xml.addAttribute("centerY", grad->centerY);
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
      xml.addAttribute("id", grad->id);
      xml.addAttribute("centerX", grad->centerX);
      xml.addAttribute("centerY", grad->centerY);
      xml.addAttribute("halfDiagonal", grad->halfDiagonal);
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
      xml.addAttribute("id", pattern->id);
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

static void writeVectorElement(XMLBuilder& xml, const VectorElementNode* node) {
  switch (node->type()) {
    case NodeType::Rectangle: {
      auto rect = static_cast<const RectangleNode*>(node);
      xml.openElement("Rectangle");
      xml.addAttribute("centerX", rect->centerX);
      xml.addAttribute("centerY", rect->centerY);
      xml.addAttribute("width", rect->width);
      xml.addAttribute("height", rect->height);
      xml.addAttribute("roundness", rect->roundness);
      xml.addAttribute("reversed", rect->reversed);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::Ellipse: {
      auto ellipse = static_cast<const EllipseNode*>(node);
      xml.openElement("Ellipse");
      xml.addAttribute("centerX", ellipse->centerX);
      xml.addAttribute("centerY", ellipse->centerY);
      xml.addAttribute("width", ellipse->width);
      xml.addAttribute("height", ellipse->height);
      xml.addAttribute("reversed", ellipse->reversed);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::Polystar: {
      auto polystar = static_cast<const PolystarNode*>(node);
      xml.openElement("Polystar");
      xml.addAttribute("centerX", polystar->centerX);
      xml.addAttribute("centerY", polystar->centerY);
      xml.addAttribute("type", PolystarTypeToString(polystar->polystarType));
      xml.addAttribute("points", polystar->points, 5.0f);
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
      if (!path->d.isEmpty()) {
        xml.addAttribute("d", path->d.toSVGString());
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
      xml.closeElementStart();
      xml.addTextContent(text->text);
      xml.closeElement();
      break;
    }
    case NodeType::Fill: {
      auto fill = static_cast<const FillNode*>(node);
      xml.openElement("Fill");
      xml.addAttribute("color", fill->color);
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
      if (fill->colorSource) {
        xml.closeElementStart();
        writeColorSource(xml, fill->colorSource.get());
        xml.closeElement();
      } else {
        xml.closeElementSelfClosing();
      }
      break;
    }
    case NodeType::Stroke: {
      auto stroke = static_cast<const StrokeNode*>(node);
      xml.openElement("Stroke");
      xml.addAttribute("color", stroke->color);
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
      if (stroke->colorSource) {
        xml.closeElementStart();
        writeColorSource(xml, stroke->colorSource.get());
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
      xml.addAttribute("radius", round->radius);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::MergePath: {
      auto merge = static_cast<const MergePathNode*>(node);
      xml.openElement("MergePath");
      if (merge->op != PathOp::Append) {
        xml.addAttribute("op", PathOpToString(merge->op));
      }
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::TextModifier: {
      auto modifier = static_cast<const TextModifierNode*>(node);
      xml.openElement("TextModifier");
      if (modifier->anchor.x != 0.5f || modifier->anchor.y != 0.5f) {
        xml.addAttribute("anchor", pointToString(modifier->anchor));
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
          xml.addAttribute("randomize", selector.randomize);
          xml.addAttribute("seed", selector.seed);
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
      xml.addAttribute("width", layout->width);
      xml.addAttribute("height", layout->height);
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
      if (repeater->anchor.x != 0 || repeater->anchor.y != 0) {
        xml.addAttribute("anchor", pointToString(repeater->anchor));
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
      if (group->anchor.x != 0 || group->anchor.y != 0) {
        xml.addAttribute("anchor", pointToString(group->anchor));
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
          writeVectorElement(xml, element.get());
        }
        xml.closeElement();
      }
      break;
    }
    default:
      break;
  }
}

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

static void writeLayerFilter(XMLBuilder& xml, const LayerFilterNode* node) {
  switch (node->type()) {
    case NodeType::BlurFilter: {
      auto filter = static_cast<const BlurFilterNode*>(node);
      xml.openElement("BlurFilter");
      xml.addAttribute("blurrinessX", filter->blurrinessX);
      xml.addAttribute("blurrinessY", filter->blurrinessY);
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

static void writeResource(XMLBuilder& xml, const ResourceNode* node) {
  switch (node->type()) {
    case NodeType::Image: {
      auto image = static_cast<const ImageNode*>(node);
      xml.openElement("Image");
      xml.addAttribute("id", image->id);
      xml.addAttribute("source", image->source);
      xml.closeElementSelfClosing();
      break;
    }
    case NodeType::Composition: {
      auto comp = static_cast<const CompositionNode*>(node);
      xml.openElement("Composition");
      xml.addAttribute("id", comp->id);
      xml.addAttribute("width", comp->width);
      xml.addAttribute("height", comp->height);
      if (comp->layers.empty()) {
        xml.closeElementSelfClosing();
      } else {
        xml.closeElementStart();
        for (const auto& layer : comp->layers) {
          writeLayer(xml, layer.get());
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
      writeColorSource(xml, static_cast<const ColorSourceNode*>(node));
      break;
    default:
      break;
  }
}

static void writeLayer(XMLBuilder& xml, const LayerNode* node) {
  xml.openElement("Layer");
  xml.addAttribute("id", node->id);
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
      writeVectorElement(xml, element.get());
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
    writeLayer(xml, child.get());
  }

  xml.closeElement();
}

std::string PAGXXMLWriter::Write(const PAGXDocument& doc) {
  XMLBuilder xml = {};
  xml.appendDeclaration();

  xml.openElement("pagx");
  xml.addAttribute("version", doc.version);
  xml.addAttribute("width", doc.width);
  xml.addAttribute("height", doc.height);
  xml.closeElementStart();

  if (!doc.resources.empty()) {
    xml.openElement("Resources");
    xml.closeElementStart();
    for (const auto& resource : doc.resources) {
      writeResource(xml, resource.get());
    }
    xml.closeElement();
  }

  for (const auto& layer : doc.layers) {
    writeLayer(xml, layer.get());
  }

  xml.closeElement();

  return xml.str();
}

}  // namespace pagx
