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
#include <unordered_set>
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

  std::unique_ptr<Layer> convertToLayer(const std::shared_ptr<DOMNode>& element,
                                            const InheritedStyle& parentStyle);
  void convertChildren(const std::shared_ptr<DOMNode>& element,
                       std::vector<std::unique_ptr<Node>>& contents,
                       const InheritedStyle& inheritedStyle);
  std::unique_ptr<Node> convertElement(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<Group> convertG(const std::shared_ptr<DOMNode>& element,
                                      const InheritedStyle& inheritedStyle);
  std::unique_ptr<Node> convertRect(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<Node> convertCircle(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<Node> convertEllipse(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<Node> convertLine(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<Node> convertPolyline(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<Node> convertPolygon(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<Node> convertPath(const std::shared_ptr<DOMNode>& element);
  std::unique_ptr<Group> convertText(const std::shared_ptr<DOMNode>& element,
                                         const InheritedStyle& inheritedStyle);
  std::unique_ptr<Node> convertUse(const std::shared_ptr<DOMNode>& element);

  std::unique_ptr<LinearGradient> convertLinearGradient(
      const std::shared_ptr<DOMNode>& element, const Rect& shapeBounds);
  std::unique_ptr<RadialGradient> convertRadialGradient(
      const std::shared_ptr<DOMNode>& element, const Rect& shapeBounds);
  std::unique_ptr<ImagePattern> convertPattern(const std::shared_ptr<DOMNode>& element,
                                                    const Rect& shapeBounds);

  std::unique_ptr<Layer> convertMaskElement(const std::shared_ptr<DOMNode>& maskElement,
                                                const InheritedStyle& parentStyle);
  void convertFilterElement(const std::shared_ptr<DOMNode>& filterElement,
                            std::vector<std::unique_ptr<Node>>& filters);

  void addFillStroke(const std::shared_ptr<DOMNode>& element,
                     std::vector<std::unique_ptr<Node>>& contents,
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
  void mergeAdjacentLayers(std::vector<std::unique_ptr<Layer>>& layers);

  // Collect all IDs from the SVG document to avoid conflicts when generating new IDs.
  void collectAllIds(const std::shared_ptr<DOMNode>& node);

  // Generate a unique ID that doesn't conflict with existing SVG IDs.
  std::string generateUniqueId(const std::string& prefix);

  // Parse data-* attributes from element and add to layer's customData.
  void parseCustomData(const std::shared_ptr<DOMNode>& element, Layer* layer);

  PAGXSVGParser::Options _options = {};
  std::shared_ptr<PAGXDocument> _document = nullptr;
  std::unordered_map<std::string, std::shared_ptr<DOMNode>> _defs = {};
  std::vector<std::unique_ptr<Layer>> _maskLayers = {};
  std::unordered_map<std::string, std::string> _imageSourceToId = {};  // Maps image source to resource ID.
  std::unordered_set<std::string> _existingIds = {};  // All IDs found in SVG to avoid conflicts.
  int _nextImageId = 0;
  int _nextGeneratedId = 0;  // Counter for generating unique IDs.
  float _viewBoxWidth = 0;
  float _viewBoxHeight = 0;
};

}  // namespace pagx
