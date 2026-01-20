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

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXSVGParser.h"

namespace pagx {

/**
 * Internal SVG XML node representation.
 */
struct SVGXMLNode {
  std::string tagName = {};
  std::unordered_map<std::string, std::string> attributes = {};
  std::vector<std::unique_ptr<SVGXMLNode>> children = {};
  std::string textContent = {};

  std::string getAttribute(const std::string& name, const std::string& defaultValue = "") const {
    auto it = attributes.find(name);
    return it != attributes.end() ? it->second : defaultValue;
  }

  bool hasAttribute(const std::string& name) const {
    return attributes.find(name) != attributes.end();
  }
};

/**
 * Internal SVG parser implementation.
 */
class SVGParserImpl {
 public:
  SVGParserImpl(const PAGXSVGParser::Options& options);

  std::shared_ptr<PAGXDocument> parse(const uint8_t* data, size_t length);

 private:
  std::unique_ptr<SVGXMLNode> parseXML(const char* data, size_t length);
  void parseSVGRoot(SVGXMLNode* svgNode);
  void parseDefs(SVGXMLNode* defsNode);
  std::unique_ptr<PAGXNode> convertElement(SVGXMLNode* element);
  std::unique_ptr<PAGXNode> convertG(SVGXMLNode* element);
  std::unique_ptr<PAGXNode> convertRect(SVGXMLNode* element);
  std::unique_ptr<PAGXNode> convertCircle(SVGXMLNode* element);
  std::unique_ptr<PAGXNode> convertEllipse(SVGXMLNode* element);
  std::unique_ptr<PAGXNode> convertLine(SVGXMLNode* element);
  std::unique_ptr<PAGXNode> convertPolyline(SVGXMLNode* element);
  std::unique_ptr<PAGXNode> convertPolygon(SVGXMLNode* element);
  std::unique_ptr<PAGXNode> convertPath(SVGXMLNode* element);
  std::unique_ptr<PAGXNode> convertText(SVGXMLNode* element);
  std::unique_ptr<PAGXNode> convertImage(SVGXMLNode* element);
  std::unique_ptr<PAGXNode> convertUse(SVGXMLNode* element);
  std::unique_ptr<PAGXNode> convertLinearGradient(SVGXMLNode* element);
  std::unique_ptr<PAGXNode> convertRadialGradient(SVGXMLNode* element);
  std::unique_ptr<PAGXNode> convertPattern(SVGXMLNode* element);
  std::unique_ptr<PAGXNode> convertMask(SVGXMLNode* element);
  std::unique_ptr<PAGXNode> convertClipPath(SVGXMLNode* element);
  std::unique_ptr<PAGXNode> convertFilter(SVGXMLNode* element);

  void parseCommonAttributes(SVGXMLNode* element, PAGXNode* node);
  void parseFillStroke(SVGXMLNode* element, PAGXNode* node);
  Matrix parseTransform(const std::string& value);
  Color parseColor(const std::string& value);
  float parseLength(const std::string& value, float containerSize = 0);
  std::vector<float> parseViewBox(const std::string& value);
  PathData parsePoints(const std::string& value, bool closed);

  std::string resolveUrl(const std::string& url);

  PAGXSVGParser::Options _options = {};
  std::shared_ptr<PAGXDocument> _document = nullptr;
  std::unordered_map<std::string, SVGXMLNode*> _defs = {};
  float _viewBoxWidth = 0;
  float _viewBoxHeight = 0;
};

}  // namespace pagx
