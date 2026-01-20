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
 * Inherited SVG style properties that cascade down the element tree.
 */
struct InheritedStyle {
  std::string fill = "";           // Empty means not set, "none" means no fill.
  std::string stroke = "";         // Empty means not set.
  std::string fillOpacity = "";    // Empty means not set.
  std::string strokeOpacity = "";  // Empty means not set.
  std::string fillRule = "";       // Empty means not set.
};

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

  std::unique_ptr<LayerNode> convertToLayer(const std::shared_ptr<DOMNode>& element,
                                            const InheritedStyle& parentStyle);
  void convertChildren(const std::shared_ptr<DOMNode>& element,
                       std::vector<std::unique_ptr<VectorElementNode>>& contents,
                       const InheritedStyle& inheritedStyle);
  std::unique_ptr<VectorElementNode> convertElement(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<GroupNode> convertG(const std::shared_ptr<DOMNode>& element,
                                      const InheritedStyle& inheritedStyle);
  std::unique_ptr<VectorElementNode> convertRect(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<VectorElementNode> convertCircle(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<VectorElementNode> convertEllipse(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<VectorElementNode> convertLine(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<VectorElementNode> convertPolyline(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<VectorElementNode> convertPolygon(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<VectorElementNode> convertPath(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<GroupNode> convertText(const std::shared_ptr<DOMNode>& element,
                                         const InheritedStyle& inheritedStyle);
  std::unique_ptr<VectorElementNode> convertUse(const std::shared_ptr<DOMNode>& element);

  std::unique_ptr<LinearGradientNode> convertLinearGradient(
      const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<RadialGradientNode> convertRadialGradient(
      const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<ImagePatternNode> convertPattern(const std::shared_ptr<DOMNode>& element,
                                                    const Rect& shapeBounds);

  std::unique_ptr<LayerNode> convertMaskElement(const std::shared_ptr<DOMNode>& maskElement,
                                                const InheritedStyle& parentStyle);
  void convertFilterElement(const std::shared_ptr<DOMNode>& filterElement,
                            std::vector<std::unique_ptr<LayerFilterNode>>& filters);

  void addFillStroke(const std::shared_ptr<DOMNode>& element,
                     std::vector<std::unique_ptr<VectorElementNode>>& contents,
                     const InheritedStyle& inheritedStyle);

  // Compute shape bounds from SVG element attributes.
  Rect getShapeBounds(const std::shared_ptr<DOMNode>& element);

  InheritedStyle computeInheritedStyle(const std::shared_ptr<DOMNode>& element,
                                       const InheritedStyle& parentStyle);

  Matrix parseTransform(const std::string& value);
  Color parseColor(const std::string& value);
  float parseLength(const std::string& value, float containerSize);
  std::vector<float> parseViewBox(const std::string& value);
  PathData parsePoints(const std::string& value, bool closed);
  std::string resolveUrl(const std::string& url);

  // Helper to get attribute from DOMNode.
  std::string getAttribute(const std::shared_ptr<DOMNode>& node, const std::string& name,
                           const std::string& defaultValue = "") const;

  // Register an image resource and return its reference ID (e.g., "#image0").
  // If the image source (data URI or path) has already been registered, returns the existing ID.
  std::string registerImageResource(const std::string& imageSource);

  // Merge adjacent layers that have the same shape geometry.
  // This optimizes the output by combining Fill and Stroke for identical shapes into one Layer.
  void mergeAdjacentLayers(std::vector<std::unique_ptr<LayerNode>>& layers);

  PAGXSVGParser::Options _options = {};
  std::shared_ptr<PAGXDocument> _document = nullptr;
  std::unordered_map<std::string, std::shared_ptr<DOMNode>> _defs = {};
  std::vector<std::unique_ptr<LayerNode>> _maskLayers = {};
  std::unordered_map<std::string, std::string> _imageSourceToId = {};  // Maps image source to resource ID.
  int _nextImageId = 0;
  float _viewBoxWidth = 0;
  float _viewBoxHeight = 0;
};

}  // namespace pagx
