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
#include "xml/XMLDOM.h"

namespace pagx {

/**
 * Internal SVG parser implementation using expat-based XML DOM parsing.
 */
class SVGParserImpl {
 public:
  explicit SVGParserImpl(const PAGXSVGParser::Options& options);

  std::shared_ptr<PAGXDocument> parse(const uint8_t* data, size_t length);
  std::shared_ptr<PAGXDocument> parseFile(const std::string& filePath);

 private:
  std::shared_ptr<PAGXDocument> parseDOM(const std::shared_ptr<DOM>& dom);

  void parseDefs(const std::shared_ptr<DOMNode>& defsNode);

  std::unique_ptr<LayerNode> convertToLayer(const std::shared_ptr<DOMNode>& element);
  void convertChildren(const std::shared_ptr<DOMNode>& element,
                       std::vector<std::unique_ptr<VectorElementNode>>& contents);
  std::unique_ptr<VectorElementNode> convertElement(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<GroupNode> convertG(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<VectorElementNode> convertRect(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<VectorElementNode> convertCircle(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<VectorElementNode> convertEllipse(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<VectorElementNode> convertLine(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<VectorElementNode> convertPolyline(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<VectorElementNode> convertPolygon(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<VectorElementNode> convertPath(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<GroupNode> convertText(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<VectorElementNode> convertUse(const std::shared_ptr<DOMNode>& element);

  std::unique_ptr<LinearGradientNode> convertLinearGradient(
      const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<RadialGradientNode> convertRadialGradient(
      const std::shared_ptr<DOMNode>& element);

  void addFillStroke(const std::shared_ptr<DOMNode>& element,
                     std::vector<std::unique_ptr<VectorElementNode>>& contents);

  Matrix parseTransform(const std::string& value);
  Color parseColor(const std::string& value);
  float parseLength(const std::string& value, float containerSize);
  std::vector<float> parseViewBox(const std::string& value);
  PathData parsePoints(const std::string& value, bool closed);
  std::string resolveUrl(const std::string& url);

  // Helper to get attribute from DOMNode.
  std::string getAttribute(const std::shared_ptr<DOMNode>& node, const std::string& name,
                           const std::string& defaultValue = "") const;

  PAGXSVGParser::Options _options = {};
  std::shared_ptr<PAGXDocument> _document = nullptr;
  std::unordered_map<std::string, std::shared_ptr<DOMNode>> _defs = {};
  float _viewBoxWidth = 0;
  float _viewBoxHeight = 0;
};

}  // namespace pagx
