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

struct CheckNode {
  std::string label;
  LCRect bounds;
  bool clipToBounds = false;
  std::string layoutMode;
  std::string layoutPositioning;
  std::vector<std::string> problems;
  std::vector<std::shared_ptr<CheckNode>> children;
};

struct LayoutOptions {
  std::string inputFile;
  std::string id;
  std::string xpath;
  bool check = false;
  bool jsonOutput = false;
};

// ============================================================================
// Geometry Helpers
// ============================================================================

static bool RectsOverlap(const LCRect& a, const LCRect& b) {
  return a.x < b.x + b.width && a.x + a.width > b.x && a.y < b.y + b.height && a.y + a.height > b.y;
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

static std::string MakeLabel(const char* typeName, const std::string& id, int index) {
  std::string label = typeName;
  if (!id.empty()) {
    label += "#";
    label += id;
  } else if (index >= 0) {
    label += "[";
    label += std::to_string(index);
    label += "]";
  }
  return label;
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

static void DetectOverlap(const std::vector<std::shared_ptr<CheckNode>>& layoutChildren,
                          const std::string& parentLayoutMode) {
  if (parentLayoutMode.empty()) {
    return;
  }
  for (size_t i = 0; i < layoutChildren.size(); ++i) {
    if (layoutChildren[i]->layoutPositioning == "ABSOLUTE") {
      continue;
    }
    for (size_t j = i + 1; j < layoutChildren.size(); ++j) {
      if (layoutChildren[j]->layoutPositioning == "ABSOLUTE") {
        continue;
      }
      if (RectsOverlap(layoutChildren[i]->bounds, layoutChildren[j]->bounds)) {
        layoutChildren[i]->problems.push_back("overlaps with " + layoutChildren[j]->label);
        layoutChildren[j]->problems.push_back("overlaps with " + layoutChildren[i]->label);
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
    if (bounds.width == 0 && bounds.height == 0 && bounds.x == 0 && bounds.y == 0) {
      continue;
    }

    auto node = std::make_shared<CheckNode>();
    node->label = MakeLabel(NodeTypeName(element->nodeType()), element->id, i);
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
  node->label = MakeLabel("Layer", layer->id, indexInParent);

  auto lb = layer->layoutBounds();
  node->bounds = {parentX + lb.x, parentY + lb.y, lb.width, lb.height};

  if (layer->clipToBounds) {
    node->clipToBounds = true;
  }
  if (layer->layout == LayoutMode::Horizontal) {
    node->layoutMode = "HORIZONTAL";
  } else if (layer->layout == LayoutMode::Vertical) {
    node->layoutMode = "VERTICAL";
  }
  if (!layer->includeInLayout) {
    node->layoutPositioning = "ABSOLUTE";
  }

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
    DetectOverlap(childLayerNodes, node->layoutMode);
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
// Problem Counting
// ============================================================================

static int CountProblems(const std::vector<std::shared_ptr<CheckNode>>& nodes) {
  int count = 0;
  for (const auto& node : nodes) {
    count += static_cast<int>(node->problems.size());
    count += CountProblems(node->children);
  }
  return count;
}

// ============================================================================
// Problem Filtering
// ============================================================================

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
// Output: JSON
// ============================================================================

static std::string EscapeJsonString(const std::string& s) {
  std::string out = "\"";
  for (char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
          out += buf;
        } else {
          out += c;
        }
    }
  }
  out += "\"";
  return out;
}

static std::string NodeToJson(const CheckNode& node) {
  std::ostringstream oss;
  oss << "{\"label\":" << EscapeJsonString(node.label);
  oss << ",\"bounds\":{\"x\":" << static_cast<int>(node.bounds.x)
      << ",\"y\":" << static_cast<int>(node.bounds.y)
      << ",\"width\":" << static_cast<int>(node.bounds.width)
      << ",\"height\":" << static_cast<int>(node.bounds.height) << "}";
  if (node.clipToBounds) {
    oss << ",\"clipToBounds\":true";
  }
  if (!node.layoutMode.empty()) {
    oss << ",\"layoutMode\":" << EscapeJsonString(node.layoutMode);
  }
  if (!node.layoutPositioning.empty()) {
    oss << ",\"layoutPositioning\":" << EscapeJsonString(node.layoutPositioning);
  }
  if (!node.problems.empty()) {
    oss << ",\"problems\":[";
    for (size_t i = 0; i < node.problems.size(); ++i) {
      if (i > 0) {
        oss << ",";
      }
      oss << EscapeJsonString(node.problems[i]);
    }
    oss << "]";
  }
  if (!node.children.empty()) {
    oss << ",\"children\":[";
    for (size_t i = 0; i < node.children.size(); ++i) {
      if (i > 0) {
        oss << ",";
      }
      oss << NodeToJson(*node.children[i]);
    }
    oss << "]";
  }
  oss << "}";
  return oss.str();
}

// ============================================================================
// Output: Text
// ============================================================================

static void PrintNodeText(const CheckNode& node, int indent) {
  std::string pad(indent * 2, ' ');
  std::cout << pad << node.label << "\n";
  std::cout << pad << "  bounds: x=" << static_cast<int>(node.bounds.x)
            << " y=" << static_cast<int>(node.bounds.y)
            << " w=" << static_cast<int>(node.bounds.width)
            << " h=" << static_cast<int>(node.bounds.height) << "\n";
  if (!node.layoutMode.empty()) {
    std::cout << pad << "  layoutMode: " << node.layoutMode << "\n";
  }
  if (!node.layoutPositioning.empty()) {
    std::cout << pad << "  layoutPositioning: " << node.layoutPositioning << "\n";
  }
  if (node.clipToBounds) {
    std::cout << pad << "  clipToBounds: true\n";
  }
  if (!node.problems.empty()) {
    std::cout << pad << "  problems:\n";
    for (const auto& p : node.problems) {
      std::cout << pad << "    - " << p << "\n";
    }
  }
  for (const auto& child : node.children) {
    PrintNodeText(*child, indent + 1);
  }
}

// ============================================================================
// Command
// ============================================================================

static void PrintUsage() {
  std::cout << "Usage: pagx layout [options] <file.pagx>\n"
            << "\n"
            << "Display layout structure of a PAGX file. By default outputs the full layout\n"
            << "tree with bounds for all Layers and elements. With --check, detects layout\n"
            << "problems and returns non-zero exit code when issues are found.\n"
            << "\n"
            << "Options:\n"
            << "  --id <id>             Limit scope to the Layer with the specified id\n"
            << "  --xpath <expr>        Limit scope to Layers matched by XPath expression\n"
            << "  --check               Detect layout problems (returns non-zero if found)\n"
            << "  --json                Output in JSON format\n"
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
    } else if (strcmp(argv[i], "--json") == 0) {
      opts->jsonOutput = true;
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
  bool hasTargetScope = !opts.id.empty() || !opts.xpath.empty();
  for (int i = 0; i < static_cast<int>(targetLayers.size()); ++i) {
    auto lb = targetLayers[i]->layoutBounds();
    float originX = hasTargetScope ? -lb.x : 0;
    float originY = hasTargetScope ? -lb.y : 0;
    auto node = BuildLayoutTree(targetLayers[i], originX, originY, i, opts.check);
    if (node) {
      results.push_back(node);
    }
  }

  int problemCount = opts.check ? CountProblems(results) : 0;

  if (opts.check) {
    results = FilterProblematicNodes(results);
  }

  // Output.
  if (opts.jsonOutput) {
    std::cout << "[";
    for (size_t i = 0; i < results.size(); ++i) {
      if (i > 0) {
        std::cout << ",";
      }
      std::cout << NodeToJson(*results[i]);
    }
    std::cout << "]\n";
  } else {
    if (results.empty() && opts.check) {
      std::cout << "No layout issues detected.\n";
    }
    for (const auto& node : results) {
      PrintNodeText(*node, 0);
    }
  }

  return problemCount > 0 ? 1 : 0;
}

}  // namespace pagx::cli
