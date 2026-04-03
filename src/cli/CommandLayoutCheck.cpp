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

#include "cli/CommandLayoutCheck.h"
#include <libxml/parser.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>
#include "cli/XPathQuery.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXImporter.h"
#include "pagx/layout/LayoutNode.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"

namespace pagx::cli {

// ============================================================================
// Data Structures
// ============================================================================

struct LCRect {
  float x = 0;
  float y = 0;
  float width = 0;
  float height = 0;
};

struct LayoutAttrs {
  std::string layoutMode;
  float gap = 0;
  float flex = 0;
  Padding padding = {};
  Alignment alignment = Alignment::Stretch;
  Arrangement arrangement = Arrangement::Start;
  bool includeInLayout = true;
  bool clipToBounds = false;
};

struct CheckNode {
  std::string tagName;
  std::string id;
  int index = -1;
  LCRect bounds;
  LayoutAttrs attrs;
  std::vector<std::string> problems;
  std::vector<std::shared_ptr<CheckNode>> children;
};

struct LayoutOptions {
  std::string inputFile;
  std::string id;
  std::string xpath;
  bool check = false;
};

// ============================================================================
// Geometry Helpers
// ============================================================================

static bool RectsOverlap(const LCRect& a, const LCRect& b) {
  return a.x < b.x + b.width && a.x + a.width > b.x && a.y < b.y + b.height &&
         a.y + a.height > b.y;
}

static bool IsFullyContained(const LCRect& parent, const LCRect& child) {
  static constexpr float TOLERANCE = 0.5f;
  return (child.x + TOLERANCE) >= parent.x && (child.y + TOLERANCE) >= parent.y &&
         (child.x + child.width) <= (parent.x + parent.width + TOLERANCE) &&
         (child.y + child.height) <= (parent.y + parent.height + TOLERANCE);
}

// ============================================================================
// Node Type Name
// ============================================================================

static const char* NodeTypeName(NodeType type) {
  switch (type) {
    case NodeType::Rectangle:
      return "Rectangle";
    case NodeType::Ellipse:
      return "Ellipse";
    case NodeType::Path:
      return "Path";
    case NodeType::Polystar:
      return "Polystar";
    case NodeType::Text:
      return "Text";
    case NodeType::TextPath:
      return "TextPath";
    case NodeType::Group:
      return "Group";
    case NodeType::TextBox:
      return "TextBox";
    default:
      return "Element";
  }
}

// ============================================================================
// Problem Detection
// ============================================================================

static void DetectZeroSize(CheckNode* node, bool check) {
  if (!check) {
    return;
  }
  if (node->bounds.width == 0 || node->bounds.height == 0) {
    std::ostringstream oss;
    oss << "zero size (width: " << static_cast<int>(node->bounds.width)
        << ", height: " << static_cast<int>(node->bounds.height) << "); node will be invisible";
    node->problems.push_back(oss.str());
  }
}

static std::string NodeLabel(const CheckNode& node) {
  if (!node.id.empty()) {
    return node.tagName + "#" + node.id;
  }
  if (node.index >= 0) {
    return node.tagName + "[" + std::to_string(node.index) + "]";
  }
  return node.tagName;
}

static void DetectOverlap(const std::vector<std::shared_ptr<CheckNode>>& layoutChildren,
                           const std::string& parentLayoutMode) {
  if (parentLayoutMode.empty()) {
    return;
  }
  for (size_t i = 0; i < layoutChildren.size(); ++i) {
    if (!layoutChildren[i]->attrs.includeInLayout) {
      continue;
    }
    for (size_t j = i + 1; j < layoutChildren.size(); ++j) {
      if (!layoutChildren[j]->attrs.includeInLayout) {
        continue;
      }
      if (RectsOverlap(layoutChildren[i]->bounds, layoutChildren[j]->bounds)) {
        layoutChildren[i]->problems.push_back("overlaps with " + NodeLabel(*layoutChildren[j]));
        layoutChildren[j]->problems.push_back("overlaps with " + NodeLabel(*layoutChildren[i]));
      }
    }
  }
}

static void DetectClippedContent(const LCRect& parentBounds,
                                  const std::vector<std::shared_ptr<CheckNode>>& children) {
  for (const auto& child : children) {
    if (!IsFullyContained(parentBounds, child->bounds)) {
      child->problems.push_back("clipped by parent (outside parent bounds)");
    }
  }
}

// ============================================================================
// Element Tree Building
// ============================================================================

static void BuildElementNodes(const std::vector<Element*>& elements,
                              std::vector<std::shared_ptr<CheckNode>>& out, float parentX,
                              float parentY, bool check) {
  for (int i = 0; i < static_cast<int>(elements.size()); ++i) {
    auto* element = elements[i];
    auto* layoutNode = pagx::LayoutNode::AsLayoutNode(element);
    if (!layoutNode) {
      continue;
    }
    auto bounds = layoutNode->layoutBounds();

    auto node = std::make_shared<CheckNode>();
    node->tagName = NodeTypeName(element->nodeType());
    node->id = element->id;
    node->index = i;
    node->bounds = {parentX + bounds.x, parentY + bounds.y, bounds.width, bounds.height};
    DetectZeroSize(node.get(), check);

    if (element->nodeType() == NodeType::Group || element->nodeType() == NodeType::TextBox) {
      auto* group = static_cast<const Group*>(element);
      if (!group->elements.empty()) {
        std::vector<std::shared_ptr<CheckNode>> groupChildren;
        BuildElementNodes(group->elements, groupChildren, node->bounds.x, node->bounds.y, check);
        if (!groupChildren.empty()) {
          node->children = std::move(groupChildren);
        }
      }
    }

    out.push_back(node);
  }
}

// ============================================================================
// Layer Tree Building
// ============================================================================

static std::shared_ptr<CheckNode> BuildLayoutTree(const Layer* layer, float parentX, float parentY,
                                                   int indexInParent, bool check) {
  if (layer == nullptr) {
    return nullptr;
  }

  auto node = std::make_shared<CheckNode>();
  node->tagName = "Layer";
  node->id = layer->id;
  node->index = indexInParent;

  auto lb = layer->layoutBounds();
  node->bounds = {parentX + lb.x, parentY + lb.y, lb.width, lb.height};

  if (layer->layout == LayoutMode::Horizontal) {
    node->attrs.layoutMode = "horizontal";
  } else if (layer->layout == LayoutMode::Vertical) {
    node->attrs.layoutMode = "vertical";
  }
  node->attrs.gap = layer->gap;
  node->attrs.flex = layer->flex;
  node->attrs.padding = layer->padding;
  node->attrs.alignment = layer->alignment;
  node->attrs.arrangement = layer->arrangement;
  node->attrs.includeInLayout = layer->includeInLayout;
  node->attrs.clipToBounds = layer->clipToBounds;

  DetectZeroSize(node.get(), check);

  std::vector<std::shared_ptr<CheckNode>> elementNodes;
  std::vector<std::shared_ptr<CheckNode>> childLayerNodes;

  if (!layer->contents.empty()) {
    BuildElementNodes(layer->contents, elementNodes, node->bounds.x, node->bounds.y, check);
  }

  for (int i = 0; i < static_cast<int>(layer->children.size()); ++i) {
    auto childNode = BuildLayoutTree(layer->children[i], node->bounds.x, node->bounds.y, i, check);
    if (childNode) {
      childLayerNodes.push_back(childNode);
    }
  }

  if (check) {
    DetectOverlap(childLayerNodes, node->attrs.layoutMode);
    if (layer->clipToBounds) {
      DetectClippedContent(node->bounds, childLayerNodes);
    }
  }

  std::vector<std::shared_ptr<CheckNode>> allChildren;
  allChildren.reserve(elementNodes.size() + childLayerNodes.size());
  allChildren.insert(allChildren.end(), elementNodes.begin(), elementNodes.end());
  allChildren.insert(allChildren.end(), childLayerNodes.begin(), childLayerNodes.end());
  if (!allChildren.empty()) {
    node->children = std::move(allChildren);
  }

  return node;
}

// ============================================================================
// Problem Counting and Filtering
// ============================================================================

static int CountProblems(const std::vector<std::shared_ptr<CheckNode>>& nodes) {
  int count = 0;
  for (const auto& node : nodes) {
    count += static_cast<int>(node->problems.size());
    count += CountProblems(node->children);
  }
  return count;
}

static std::vector<std::shared_ptr<CheckNode>> FilterProblematicNodes(
    const std::vector<std::shared_ptr<CheckNode>>& nodes) {
  std::vector<std::shared_ptr<CheckNode>> result;
  for (const auto& node : nodes) {
    auto filtered = std::make_shared<CheckNode>(*node);
    if (!filtered->children.empty()) {
      filtered->children = FilterProblematicNodes(filtered->children);
    }
    bool hasProblems = !filtered->problems.empty();
    bool hasProblematicChildren = !filtered->children.empty();
    if (hasProblems || hasProblematicChildren) {
      if (!hasProblematicChildren) {
        filtered->children.clear();
      }
      result.push_back(filtered);
    }
  }
  return result;
}

// ============================================================================
// XML Output
// ============================================================================

static std::string EscapeXmlAttr(const std::string& s) {
  std::string out;
  for (char c : s) {
    switch (c) {
      case '&':
        out += "&amp;";
        break;
      case '"':
        out += "&quot;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      default:
        out += c;
    }
  }
  return out;
}

static std::string FormatInt(float v) {
  return std::to_string(static_cast<int>(v));
}

static void PrintNodeXml(std::ostream& os, const CheckNode& node, int indent) {
  std::string pad(indent * 2, ' ');
  os << pad << "<" << node.tagName;

  // id attribute
  if (!node.id.empty()) {
    os << " id=\"" << EscapeXmlAttr(node.id) << "\"";
  }

  // bounds attribute
  os << " bounds=\"" << FormatInt(node.bounds.x) << "," << FormatInt(node.bounds.y) << ","
     << FormatInt(node.bounds.width) << "," << FormatInt(node.bounds.height) << "\"";

  // Layout attributes (only non-default values, only for Layer nodes)
  if (node.tagName == "Layer") {
    const auto& a = node.attrs;
    if (!a.layoutMode.empty()) {
      os << " layout=\"" << a.layoutMode << "\"";
    }
    if (a.gap != 0) {
      os << " gap=\"" << FormatInt(a.gap) << "\"";
    }
    if (a.flex != 0) {
      os << " flex=\"" << FormatInt(a.flex) << "\"";
    }
    if (!a.padding.isZero()) {
      bool allEqual = (a.padding.top == a.padding.right && a.padding.right == a.padding.bottom &&
                       a.padding.bottom == a.padding.left);
      bool vhEqual = (a.padding.top == a.padding.bottom && a.padding.left == a.padding.right);
      if (allEqual) {
        os << " padding=\"" << FormatInt(a.padding.top) << "\"";
      } else if (vhEqual) {
        os << " padding=\"" << FormatInt(a.padding.top) << "," << FormatInt(a.padding.left) << "\"";
      } else {
        os << " padding=\"" << FormatInt(a.padding.top) << "," << FormatInt(a.padding.right) << ","
           << FormatInt(a.padding.bottom) << "," << FormatInt(a.padding.left) << "\"";
      }
    }
    if (a.alignment != Alignment::Stretch) {
      switch (a.alignment) {
        case Alignment::Start:
          os << " alignment=\"start\"";
          break;
        case Alignment::Center:
          os << " alignment=\"center\"";
          break;
        case Alignment::End:
          os << " alignment=\"end\"";
          break;
        default:
          break;
      }
    }
    if (a.arrangement != Arrangement::Start) {
      switch (a.arrangement) {
        case Arrangement::Center:
          os << " arrangement=\"center\"";
          break;
        case Arrangement::End:
          os << " arrangement=\"end\"";
          break;
        case Arrangement::SpaceBetween:
          os << " arrangement=\"spaceBetween\"";
          break;
        case Arrangement::SpaceEvenly:
          os << " arrangement=\"spaceEvenly\"";
          break;
        case Arrangement::SpaceAround:
          os << " arrangement=\"spaceAround\"";
          break;
        default:
          break;
      }
    }
    if (!a.includeInLayout) {
      os << " includeInLayout=\"false\"";
    }
    if (a.clipToBounds) {
      os << " clipToBounds=\"true\"";
    }
  }

  bool hasChildren = !node.problems.empty() || !node.children.empty();
  if (!hasChildren) {
    os << "/>\n";
    return;
  }

  os << ">\n";

  // Problem nodes first
  for (const auto& p : node.problems) {
    os << pad << "  <Problem>" << EscapeXmlAttr(p) << "</Problem>\n";
  }

  // Child nodes
  for (const auto& child : node.children) {
    PrintNodeXml(os, *child, indent + 1);
  }

  os << pad << "</" << node.tagName << ">\n";
}

// ============================================================================
// Command
// ============================================================================

static void PrintUsage() {
  std::cout << "Usage: pagx layout [options] <file.pagx>\n"
            << "\n"
            << "Display layout structure of a PAGX file in XML format. By default outputs the\n"
            << "full layout tree with bounds for all Layers and elements. With --check, detects\n"
            << "layout problems and returns non-zero exit code when issues are found.\n"
            << "\n"
            << "Options:\n"
            << "  --id <id>             Limit scope to the Layer with the specified id\n"
            << "  --xpath <expr>        Limit scope to Layers matched by XPath expression\n"
            << "  --check               Detect layout problems (returns non-zero if found)\n"
            << "  -h, --help            Show this help message\n";
}

static int ParseOptions(int argc, char* argv[], LayoutOptions* opts) {
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--id") == 0) {
      if (i + 1 >= argc) {
        std::cerr << "pagx layout: --id requires an argument\n";
        return 1;
      }
      opts->id = argv[++i];
    } else if (strcmp(argv[i], "--xpath") == 0) {
      if (i + 1 >= argc) {
        std::cerr << "pagx layout: --xpath requires an argument\n";
        return 1;
      }
      opts->xpath = argv[++i];
    } else if (strcmp(argv[i], "--check") == 0) {
      opts->check = true;
    } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      PrintUsage();
      return -1;
    } else if (argv[i][0] == '-') {
      std::cerr << "pagx layout: unknown option '" << argv[i] << "'\n";
      return 1;
    } else {
      opts->inputFile = argv[i];
    }
  }
  if (opts->inputFile.empty()) {
    std::cerr << "pagx layout: missing input file\n";
    return 1;
  }
  if (!opts->id.empty() && !opts->xpath.empty()) {
    std::cerr << "pagx layout: --id and --xpath are mutually exclusive\n";
    return 1;
  }
  return 0;
}

int RunLayout(int argc, char* argv[]) {
  LayoutOptions opts;
  int rc = ParseOptions(argc, argv, &opts);
  if (rc != 0) {
    return rc == -1 ? 0 : rc;
  }

  auto document = PAGXImporter::FromFile(opts.inputFile);
  if (document == nullptr) {
    std::cerr << "pagx layout: failed to load '" << opts.inputFile << "'\n";
    return 1;
  }
  for (const auto& err : document->errors) {
    std::cerr << "pagx layout: warning: " << err << "\n";
  }

  document->applyLayout();

  // Resolve target layers.
  std::vector<const Layer*> targetLayers;
  if (!opts.id.empty()) {
    auto* layer = document->findNode<Layer>(opts.id);
    if (layer == nullptr) {
      std::cerr << "pagx layout: node not found: " << opts.id << "\n";
      return 1;
    }
    targetLayers.push_back(layer);
  } else if (!opts.xpath.empty()) {
    auto xmlDoc = xmlReadFile(opts.inputFile.c_str(), nullptr, XML_PARSE_NONET);
    if (xmlDoc == nullptr) {
      std::cerr << "pagx layout: failed to parse XML from '" << opts.inputFile << "'\n";
      return 1;
    }
    auto matched = EvaluateXPath(xmlDoc, opts.xpath, document.get());
    xmlFreeDoc(xmlDoc);
    if (matched.empty()) {
      std::cerr << "pagx layout: no layers matched xpath '" << opts.xpath << "'\n";
      return 1;
    }
    for (auto* layer : matched) {
      targetLayers.push_back(layer);
    }
  } else {
    for (const auto* layer : document->layers) {
      targetLayers.push_back(layer);
    }
  }

  // Build layout tree. Use the target layer's own position as the coordinate origin so that
  // bounds are output relative to the specified root (matching --id / --xpath expectation).
  std::vector<std::shared_ptr<CheckNode>> results;
  bool scopedQuery = !opts.id.empty() || !opts.xpath.empty();
  for (int i = 0; i < static_cast<int>(targetLayers.size()); ++i) {
    auto lb = targetLayers[i]->layoutBounds();
    float originX = scopedQuery ? -lb.x : 0;
    float originY = scopedQuery ? -lb.y : 0;
    auto node = BuildLayoutTree(targetLayers[i], originX, originY, i, opts.check);
    if (node) {
      results.push_back(node);
    }
  }

  int problemCount = opts.check ? CountProblems(results) : 0;

  if (opts.check) {
    results = FilterProblematicNodes(results);
  }

  // Output XML.
  bool hasTargetScope = !opts.id.empty() || !opts.xpath.empty();
  std::cout << "<layout>\n";
  bool emptyCheck = results.empty() && opts.check;
  if (!hasTargetScope && !emptyCheck) {
    auto docWidth = static_cast<int>(document->width);
    auto docHeight = static_cast<int>(document->height);
    std::cout << "  <pagx width=\"" << docWidth << "\" height=\"" << docHeight << "\">\n";
  }
  if (emptyCheck) {
    std::cout << "  <!-- No layout problems detected. -->\n";
  }
  int contentIndent = hasTargetScope ? 1 : 2;
  for (const auto& node : results) {
    PrintNodeXml(std::cout, *node, contentIndent);
  }
  if (!hasTargetScope && !emptyCheck) {
    std::cout << "  </pagx>\n";
  }
  std::cout << "</layout>\n";

  return problemCount > 0 ? 1 : 0;
}

}  // namespace pagx::cli
