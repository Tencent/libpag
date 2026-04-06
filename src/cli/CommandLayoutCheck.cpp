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
#include <cfloat>
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
#include "pagx/nodes/TextBox.h"

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
  bool placeholder = false;
  std::vector<std::string> problems;
  std::vector<std::shared_ptr<CheckNode>> children;
};

struct LayoutOptions {
  std::string inputFile;
  std::string id;
  std::string xpath;
  int depth = 0;
  bool problemsOnly = false;
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

// ============================================================================
// Problem Detection
// ============================================================================

static void DetectZeroSize(CheckNode* node, bool check, const Layer* layer = nullptr,
                           const Layer* parentLayer = nullptr) {
  if (!check) {
    return;
  }
  if (node->bounds.width != 0 && node->bounds.height != 0) {
    return;
  }
  // Skip nodes with explicit zero size attributes (intentional, e.g. TextBox anchor mode),
  // but NOT when the Layer participates in a parent container layout — explicit zero size
  // in a layout flow is likely a mistake since the element becomes invisible.
  if (layer != nullptr) {
    bool inParentLayout =
        parentLayer != nullptr && parentLayer->layout != LayoutMode::None && layer->includeInLayout;
    if (!inParentLayout) {
      bool zeroW = (node->bounds.width == 0 && !std::isnan(layer->width) && layer->width == 0);
      bool zeroH = (node->bounds.height == 0 && !std::isnan(layer->height) && layer->height == 0);
      if (zeroW || zeroH) {
        return;
      }
    }
  }
  std::ostringstream oss;
  oss << "zero size (" << static_cast<int>(node->bounds.width) << "x"
      << static_cast<int>(node->bounds.height) << ")";
  // Analyze cause for Layer nodes.
  if (layer != nullptr && parentLayer != nullptr && layer->flex > 0 &&
      parentLayer->layout != LayoutMode::None) {
    bool horizontal = parentLayer->layout == LayoutMode::Horizontal;
    float parentExplicitMain = horizontal ? parentLayer->width : parentLayer->height;
    bool parentMainFromConstraints =
        horizontal ? (!std::isnan(parentLayer->left) && !std::isnan(parentLayer->right))
                   : (!std::isnan(parentLayer->top) && !std::isnan(parentLayer->bottom));
    if (std::isnan(parentExplicitMain) && !parentMainFromConstraints) {
      oss << ": flex child, parent has no main-axis size to distribute";
    }
  }
  node->problems.push_back(oss.str());
}

static void DetectConstraintConflicts(const Layer* layer,
                                      const std::vector<std::shared_ptr<CheckNode>>& childNodes,
                                      bool check) {
  if (!check || layer == nullptr || layer->layout == LayoutMode::None) {
    return;
  }

  // Constraint attributes on a child Layer are only effective when the parent has no container
  // layout or the child has includeInLayout="false". When a child participates in container
  // layout flow (includeInLayout=true, parent has container layout), constraints are silently
  // ignored — this is almost always unintentional.
  for (size_t idx = 0; idx < layer->children.size() && idx < childNodes.size(); ++idx) {
    auto* child = layer->children[idx];
    if (child == nullptr || !child->includeInLayout) {
      continue;
    }
    bool hasConstraints =
        (!std::isnan(child->left) || !std::isnan(child->right) || !std::isnan(child->centerX) ||
         !std::isnan(child->top) || !std::isnan(child->bottom) || !std::isnan(child->centerY));
    if (hasConstraints) {
      std::string attrs;
      if (!std::isnan(child->left)) attrs += "left ";
      if (!std::isnan(child->right)) attrs += "right ";
      if (!std::isnan(child->centerX)) attrs += "centerX ";
      if (!std::isnan(child->top)) attrs += "top ";
      if (!std::isnan(child->bottom)) attrs += "bottom ";
      if (!std::isnan(child->centerY)) attrs += "centerY ";
      if (!attrs.empty() && attrs.back() == ' ') {
        attrs.pop_back();
      }
      childNodes[idx]->problems.push_back("constraints (" + attrs +
                                          ") ignored by parent container layout");
    }
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

static void DetectFlexInContentMeasuredParent(
    const Layer* parentLayer, const std::vector<std::shared_ptr<CheckNode>>& childNodes,
    bool check) {
  if (!check || parentLayer == nullptr || parentLayer->layout == LayoutMode::None) {
    return;
  }
  bool horizontal = parentLayer->layout == LayoutMode::Horizontal;
  float parentExplicitMain = horizontal ? parentLayer->width : parentLayer->height;
  // If the parent has an explicit main-axis size, flex distribution works fine.
  if (!std::isnan(parentExplicitMain)) {
    return;
  }
  // Also check if parent has opposite-pair constraints that derive the main-axis size.
  if (horizontal && !std::isnan(parentLayer->left) && !std::isnan(parentLayer->right)) {
    return;
  }
  if (!horizontal && !std::isnan(parentLayer->top) && !std::isnan(parentLayer->bottom)) {
    return;
  }
  const char* axis = horizontal ? "width" : "height";
  for (size_t idx = 0; idx < parentLayer->children.size() && idx < childNodes.size(); ++idx) {
    auto* child = parentLayer->children[idx];
    if (child == nullptr || !child->includeInLayout || child->flex <= 0) {
      continue;
    }
    float explicitMain = horizontal ? child->width : child->height;
    if (!std::isnan(explicitMain)) {
      continue;  // Has explicit size, flex is ignored.
    }
    // Skip if the child is already zero-size — the zero-size cause analysis covers this case.
    if (childNodes[idx]->bounds.width == 0 || childNodes[idx]->bounds.height == 0) {
      continue;
    }
    // Skip if the flex child actually received a non-zero main-axis size from the layout engine.
    // This happens when the parent's main-axis size is derived from a grandparent (e.g., flex or
    // stretch from the parent's own parent), even though the parent has no explicit size attribute.
    float childResolvedMain =
        horizontal ? childNodes[idx]->bounds.width : childNodes[idx]->bounds.height;
    if (childResolvedMain > 0) {
      continue;
    }
    std::ostringstream oss;
    oss << "flex child in content-measured parent (no " << axis << "): no space to distribute";
    childNodes[idx]->problems.push_back(oss.str());
  }
}

static void DetectContentOriginOffset(const std::vector<Element*>& elements, CheckNode* container,
                                      const LayoutNode* containerNode = nullptr) {
  // Skip if the container has constraint-derived or explicit size — content offset is
  // only a problem when the container is truly content-measured (size from children).
  if (containerNode != nullptr) {
    bool hasWidthConstraint = !std::isnan(containerNode->left) && !std::isnan(containerNode->right);
    bool hasHeightConstraint =
        !std::isnan(containerNode->top) && !std::isnan(containerNode->bottom);
    if (hasWidthConstraint || hasHeightConstraint) {
      return;
    }
  }
  // If any child has constraint attributes, the positioning is intentional (e.g., padding via
  // left/top, centering via centerX/centerY). Skip detection for the entire container because
  // constrained children define the intended content region, making offsets of other children
  // within that region acceptable.
  for (auto* element : elements) {
    auto* node = LayoutNode::AsLayoutNode(element);
    if (node != nullptr && node->hasConstraints()) {
      return;
    }
  }
  // Find the minimum x and y of all unconstrained children relative to the container origin.
  // Container measurement starts from (0,0), so any gap between (0,0) and the content's
  // top-left pixel inflates the measured size and offsets the content.
  float minX = FLT_MAX;
  float minY = FLT_MAX;
  bool hasChild = false;
  for (auto* element : elements) {
    auto* layoutNode = LayoutNode::AsLayoutNode(element);
    if (layoutNode == nullptr) {
      continue;
    }
    auto bounds = layoutNode->layoutBounds();
    if (bounds.width == 0 && bounds.height == 0) {
      continue;
    }
    minX = std::min(minX, bounds.x);
    minY = std::min(minY, bounds.y);
    hasChild = true;
  }
  if (!hasChild) {
    return;
  }
  static constexpr float TOLERANCE = 0.5f;
  bool xOff = std::abs(minX) > TOLERANCE;
  bool yOff = std::abs(minY) > TOLERANCE;
  if (!xOff && !yOff) {
    return;
  }
  std::ostringstream oss;
  oss << "children start at (" << static_cast<int>(minX) << ", " << static_cast<int>(minY)
      << "), not (0, 0): container measurement inaccurate."
         " Fix: uniformly shift all children so top-left starts at (0,0),"
         " preserving relative positions";
  container->problems.push_back(oss.str());
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

static void DetectRedundantConstraints(const LayoutNode* layoutNode, CheckNode* node, bool check,
                                       bool checkDefaults = false) {
  if (!check || layoutNode == nullptr) {
    return;
  }

  // Rule A: centerX overrides left and right.
  if (!std::isnan(layoutNode->centerX)) {
    if (!std::isnan(layoutNode->left) && !std::isnan(layoutNode->right)) {
      node->problems.push_back("left/right ignored, overridden by centerX");
    } else if (!std::isnan(layoutNode->left)) {
      node->problems.push_back("left ignored, overridden by centerX");
    } else if (!std::isnan(layoutNode->right)) {
      node->problems.push_back("right ignored, overridden by centerX");
    }
  }

  // Rule A: centerY overrides top and bottom.
  if (!std::isnan(layoutNode->centerY)) {
    if (!std::isnan(layoutNode->top) && !std::isnan(layoutNode->bottom)) {
      node->problems.push_back("top/bottom ignored, overridden by centerY");
    } else if (!std::isnan(layoutNode->top)) {
      node->problems.push_back("top ignored, overridden by centerY");
    } else if (!std::isnan(layoutNode->bottom)) {
      node->problems.push_back("bottom ignored, overridden by centerY");
    }
  }

  // Rule B (Layer only): left=0 with no right/centerX is equivalent to default x=0.
  if (checkDefaults) {
    if (!std::isnan(layoutNode->left) && layoutNode->left == 0.0f &&
        std::isnan(layoutNode->right) && std::isnan(layoutNode->centerX)) {
      node->problems.push_back("left=0 with no opposite constraint is equivalent to default");
    }
    if (!std::isnan(layoutNode->top) && layoutNode->top == 0.0f && std::isnan(layoutNode->bottom) &&
        std::isnan(layoutNode->centerY)) {
      node->problems.push_back("top=0 with no opposite constraint is equivalent to default");
    }
  }
}

static void DetectClippedContent(const LCRect& parentBounds,
                                 const std::vector<std::shared_ptr<CheckNode>>& children) {
  for (const auto& child : children) {
    if (!IsFullyContained(parentBounds, child->bounds)) {
      child->problems.push_back("outside parent bounds, clipped");
    }
  }
}

static void DetectContainerOverflow(const Layer* parentLayer,
                                    const std::vector<std::shared_ptr<CheckNode>>& childNodes,
                                    bool check) {
  if (!check || parentLayer == nullptr || parentLayer->layout == LayoutMode::None) {
    return;
  }
  bool horizontal = parentLayer->layout == LayoutMode::Horizontal;
  float explicitMain = horizontal ? parentLayer->width : parentLayer->height;
  // Only check when the parent has a known main-axis size (explicit or from constraints).
  bool mainFromConstraints =
      horizontal ? (!std::isnan(parentLayer->left) && !std::isnan(parentLayer->right))
                 : (!std::isnan(parentLayer->top) && !std::isnan(parentLayer->bottom));
  if (std::isnan(explicitMain) && !mainFromConstraints) {
    return;  // Content-measured parent — size adapts to children, no overflow possible.
  }
  float paddingStart = horizontal ? parentLayer->padding.left : parentLayer->padding.top;
  float paddingEnd = horizontal ? parentLayer->padding.right : parentLayer->padding.bottom;
  float containerMain =
      horizontal ? parentLayer->layoutBounds().width : parentLayer->layoutBounds().height;
  float availableMain = containerMain - paddingStart - paddingEnd;
  float totalFixed = 0;
  size_t visibleCount = 0;
  bool hasFlex = false;
  for (size_t idx = 0; idx < parentLayer->children.size(); ++idx) {
    auto* child = parentLayer->children[idx];
    if (child == nullptr || !child->includeInLayout) {
      continue;
    }
    visibleCount++;
    float childExplicitMain = horizontal ? child->width : child->height;
    if (!std::isnan(childExplicitMain)) {
      totalFixed += childExplicitMain;
    } else if (child->flex > 0) {
      hasFlex = true;
    } else {
      float measured = horizontal ? child->layoutBounds().width : child->layoutBounds().height;
      totalFixed += measured;
    }
  }
  if (hasFlex) {
    return;  // Flex children absorb overflow.
  }
  float totalGap = parentLayer->gap * static_cast<float>(visibleCount > 1 ? visibleCount - 1 : 0);
  float totalRequired = totalFixed + totalGap;
  static constexpr float TOLERANCE = 0.5f;
  if (totalRequired > availableMain + TOLERANCE) {
    std::ostringstream oss;
    oss << "children overflow: total main-axis size (" << static_cast<int>(totalRequired)
        << ") exceeds available space (" << static_cast<int>(availableMain) << ")";
    // Report on the parent container node. Find it by matching: childNodes are siblings,
    // the parent is the node being built. We push to the parent via a pointer.
    // Since this is called from BuildLayoutTree with the parent's node, we need a different
    // approach — report on the last child that extends beyond the boundary.
    for (size_t idx = childNodes.size(); idx > 0; --idx) {
      if (childNodes[idx - 1]->attrs.includeInLayout) {
        childNodes[idx - 1]->problems.push_back(oss.str());
        break;
      }
    }
  }
}

static void DetectNegativeConstraintSize(const LayoutNode* layoutNode, CheckNode* node,
                                         float containerW, float containerH, bool check) {
  if (!check || layoutNode == nullptr) {
    return;
  }
  // Check horizontal opposite-pair.
  if (!std::isnan(layoutNode->left) && !std::isnan(layoutNode->right) && !std::isnan(containerW)) {
    float derived = containerW - layoutNode->left - layoutNode->right;
    if (derived < 0) {
      std::ostringstream oss;
      oss << "left(" << static_cast<int>(layoutNode->left) << ") + right("
          << static_cast<int>(layoutNode->right)
          << ") = " << static_cast<int>(layoutNode->left + layoutNode->right)
          << " exceeds parent width(" << static_cast<int>(containerW) << "): negative derived size";
      node->problems.push_back(oss.str());
    }
  }
  // Check vertical opposite-pair.
  if (!std::isnan(layoutNode->top) && !std::isnan(layoutNode->bottom) && !std::isnan(containerH)) {
    float derived = containerH - layoutNode->top - layoutNode->bottom;
    if (derived < 0) {
      std::ostringstream oss;
      oss << "top(" << static_cast<int>(layoutNode->top) << ") + bottom("
          << static_cast<int>(layoutNode->bottom)
          << ") = " << static_cast<int>(layoutNode->top + layoutNode->bottom)
          << " exceeds parent height(" << static_cast<int>(containerH)
          << "): negative derived size";
      node->problems.push_back(oss.str());
    }
  }
}

static void DetectIneffectiveCentering(const std::vector<Element*>& elements,
                                       const std::vector<std::shared_ptr<CheckNode>>& childNodes) {
  for (size_t i = 0; i < elements.size() && i < childNodes.size(); ++i) {
    auto* layoutNode = LayoutNode::AsLayoutNode(elements[i]);
    if (layoutNode == nullptr) {
      continue;
    }
    bool hasCenterX = !std::isnan(layoutNode->centerX);
    bool hasCenterY = !std::isnan(layoutNode->centerY);
    if (!hasCenterX && !hasCenterY) {
      continue;
    }
    auto type = elements[i]->nodeType();
    if (type == NodeType::Group || type == NodeType::TextBox) {
      continue;
    }
    if (hasCenterX) {
      childNodes[i]->problems.push_back(
          "centerX ineffective: parent is content-measured, centering relative to own size."
          " Fix: move centerX to the Group/Layer itself");
    }
    if (hasCenterY) {
      childNodes[i]->problems.push_back(
          "centerY ineffective: parent is content-measured, centering relative to own size."
          " Fix: move centerY to the Group/Layer itself");
    }
  }
}

// ============================================================================
// Element Tree Building
// ============================================================================

static void BuildElementNodes(const std::vector<Element*>& elements,
                              std::vector<std::shared_ptr<CheckNode>>& out, float parentX,
                              float parentY, float containerW, float containerH,
                              const TextBox* parentTextBox, bool check, int maxDepth,
                              int currentDepth) {
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
    // Zero-size detection: only for Group/TextBox that act as layout containers (have children
    // using size-dependent constraints on the zero axis). Leaf elements and pure-rendering Groups
    // are skipped. Per-axis: width=0 checks right/centerX, height=0 checks bottom/centerY.
    if (element->nodeType() == NodeType::Group || element->nodeType() == NodeType::TextBox) {
      auto* group = static_cast<const Group*>(element);
      bool explicitZeroW =
          (node->bounds.width == 0 && !std::isnan(group->width) && group->width == 0);
      bool explicitZeroH =
          (node->bounds.height == 0 && !std::isnan(group->height) && group->height == 0);
      if (!explicitZeroW && !explicitZeroH &&
          (node->bounds.width == 0 || node->bounds.height == 0)) {
        bool zeroW = node->bounds.width == 0;
        bool zeroH = node->bounds.height == 0;
        bool needsCheck = false;
        for (auto* child : group->elements) {
          auto* childNode = LayoutNode::AsLayoutNode(child);
          if (childNode == nullptr) {
            continue;
          }
          if ((zeroW && (!std::isnan(childNode->right) || !std::isnan(childNode->centerX))) ||
              (zeroH && (!std::isnan(childNode->bottom) || !std::isnan(childNode->centerY)))) {
            needsCheck = true;
            break;
          }
        }
        if (needsCheck) {
          DetectZeroSize(node.get(), check);
        }
      }
    }
    DetectRedundantConstraints(layoutNode, node.get(), check);
    DetectNegativeConstraintSize(layoutNode, node.get(), containerW, containerH, check);

    if (element->nodeType() == NodeType::Group || element->nodeType() == NodeType::TextBox) {
      auto* group = static_cast<const Group*>(element);
      const TextBox* nextParentTextBox = parentTextBox;
      if (element->nodeType() == NodeType::TextBox) {
        auto* textBox = static_cast<const TextBox*>(element);
        if (check && parentTextBox != nullptr) {
          std::ostringstream oss;
          oss << "nested TextBox, layout overridden by outer TextBox";
          if (!parentTextBox->id.empty()) {
            oss << " (id='" << parentTextBox->id << "')";
          }
          node->problems.push_back(oss.str());
        }
        nextParentTextBox = textBox;
      }

      if (!group->elements.empty()) {
        std::vector<std::shared_ptr<CheckNode>> groupChildren;
        BuildElementNodes(group->elements, groupChildren, node->bounds.x, node->bounds.y,
                          bounds.width, bounds.height, nextParentTextBox, check, maxDepth,
                          currentDepth);
        if (!groupChildren.empty()) {
          node->children = std::move(groupChildren);
        }
        // Check content-measured Group/TextBox containers.
        if (check && std::isnan(group->width) && std::isnan(group->height)) {
          bool sizeFromConstraints = (!std::isnan(group->left) && !std::isnan(group->right)) &&
                                     (!std::isnan(group->top) && !std::isnan(group->bottom));
          if (!sizeFromConstraints) {
            // Content origin offset: only report when the inaccurate measurement actually
            // matters. For Group/TextBox, this requires size-dependent constraints (right,
            // bottom, centerX, centerY). Pure left/top positioning is safe.
            bool hasSizeDependentConstraint =
                (!std::isnan(group->right) || !std::isnan(group->bottom) ||
                 !std::isnan(group->centerX) || !std::isnan(group->centerY));
            if (hasSizeDependentConstraint) {
              DetectContentOriginOffset(group->elements, node.get(), group);
            }
            // Ineffective centering: only check when container is truly content-measured.
            DetectIneffectiveCentering(group->elements, node->children);
          }
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
                                                  int indexInParent, bool check, int maxDepth,
                                                  int currentDepth = 0,
                                                  const Layer* parentLayer = nullptr) {
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

  // Zero-size detection: only for Layers where zero size would cause downstream layout failures
  // or affect sibling positioning. Check per-axis: width=0 only matters if children depend on
  // the horizontal axis (right/centerX), height=0 only if they depend on vertical (bottom/centerY).
  if (check && (node->bounds.width == 0 || node->bounds.height == 0)) {
    bool needsCheck = layer->layout != LayoutMode::None || layer->clipToBounds;
    // Layer participates in parent container layout — zero size affects sibling positioning.
    if (!needsCheck && parentLayer != nullptr && parentLayer->layout != LayoutMode::None &&
        layer->includeInLayout) {
      needsCheck = true;
    }
    if (!needsCheck) {
      bool zeroW = node->bounds.width == 0;
      bool zeroH = node->bounds.height == 0;
      for (auto* child : layer->children) {
        if (child == nullptr) {
          continue;
        }
        if ((zeroW && (!std::isnan(child->right) || !std::isnan(child->centerX))) ||
            (zeroH && (!std::isnan(child->bottom) || !std::isnan(child->centerY))) ||
            child->flex > 0) {
          needsCheck = true;
          break;
        }
      }
      if (!needsCheck) {
        for (auto* element : layer->contents) {
          auto* layoutNode = LayoutNode::AsLayoutNode(element);
          if (layoutNode == nullptr) {
            continue;
          }
          if ((zeroW && (!std::isnan(layoutNode->right) || !std::isnan(layoutNode->centerX))) ||
              (zeroH && (!std::isnan(layoutNode->bottom) || !std::isnan(layoutNode->centerY)))) {
            needsCheck = true;
            break;
          }
        }
      }
    }
    if (needsCheck) {
      DetectZeroSize(node.get(), check, layer, parentLayer);
    }
  }
  DetectRedundantConstraints(layer, node.get(), check, true);

  std::vector<std::shared_ptr<CheckNode>> elementNodes;
  std::vector<std::shared_ptr<CheckNode>> childLayerNodes;

  if (!layer->contents.empty()) {
    BuildElementNodes(layer->contents, elementNodes, node->bounds.x, node->bounds.y,
                      node->bounds.width, node->bounds.height, nullptr, check, maxDepth,
                      currentDepth);
    // Check content-measured Layer containers.
    // A Layer's size is content-measured when both width and height are unset AND not engine-
    // assigned. Skip if: opposite-pair constraints derive both axes, or flex assigns main-axis
    // (with default stretch assigning cross-axis, both axes are covered).
    if (check && std::isnan(layer->width) && std::isnan(layer->height)) {
      bool sizeFromConstraints = (!std::isnan(layer->left) && !std::isnan(layer->right)) &&
                                 (!std::isnan(layer->top) && !std::isnan(layer->bottom));
      bool sizeFromFlex = layer->flex > 0 && parentLayer != nullptr &&
                          parentLayer->layout != LayoutMode::None && layer->includeInLayout;
      if (!sizeFromConstraints && !sizeFromFlex) {
        // Content origin offset: only report when the inaccurate measurement actually matters.
        // That requires BOTH conditions:
        // 1. The Layer participates in layout flow or uses size-dependent constraints, AND
        // 2. The measured size feeds into positioning or sibling layout.
        // Pure left/top positioning without opposite constraints or parent layout is safe —
        // the measured size doesn't affect the Layer's or siblings' positions.
        bool inParentLayout = parentLayer != nullptr && parentLayer->layout != LayoutMode::None &&
                              layer->includeInLayout;
        bool hasSizeDependentConstraint =
            (!std::isnan(layer->right) || !std::isnan(layer->bottom) ||
             !std::isnan(layer->centerX) || !std::isnan(layer->centerY));
        if (inParentLayout || hasSizeDependentConstraint) {
          DetectContentOriginOffset(layer->contents, node.get(), layer);
        }
        // Ineffective centering: always check — children using centerX/centerY
        // inside a content-measured container is always a no-op.
        DetectIneffectiveCentering(layer->contents, elementNodes);
      }
    }
  }

  if (maxDepth <= 0 || currentDepth < maxDepth) {
    for (int i = 0; i < static_cast<int>(layer->children.size()); ++i) {
      auto childNode = BuildLayoutTree(layer->children[i], node->bounds.x, node->bounds.y, i, check,
                                       maxDepth, currentDepth + 1, layer);
      if (childNode) {
        childLayerNodes.push_back(childNode);
      }
    }
  }

  if (check) {
    DetectConstraintConflicts(layer, childLayerNodes, check);
    DetectFlexInContentMeasuredParent(layer, childLayerNodes, check);
    DetectContainerOverflow(layer, childLayerNodes, check);
    DetectOverlap(childLayerNodes, node->attrs.layoutMode);
    if (layer->clipToBounds) {
      DetectClippedContent(node->bounds, childLayerNodes);
    }
    // Detect negative constraint sizes on child Layers that are positioned by constraints
    // (not in container layout flow).
    for (size_t idx = 0; idx < layer->children.size() && idx < childLayerNodes.size(); ++idx) {
      auto* child = layer->children[idx];
      bool inFlexFlow = (layer->layout != LayoutMode::None && child->includeInLayout);
      if (!inFlexFlow) {
        DetectNegativeConstraintSize(child, childLayerNodes[idx].get(), node->bounds.width,
                                     node->bounds.height, check);
      }
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

static void DetectOffCanvas(const std::vector<std::shared_ptr<CheckNode>>& topNodes,
                            float canvasWidth, float canvasHeight) {
  LCRect canvas = {0, 0, canvasWidth, canvasHeight};
  for (const auto& node : topNodes) {
    if (node->bounds.width <= 0 || node->bounds.height <= 0) {
      continue;  // already reported as zero-size
    }
    bool intersects = node->bounds.x < canvas.x + canvas.width &&
                      node->bounds.x + node->bounds.width > canvas.x &&
                      node->bounds.y < canvas.y + canvas.height &&
                      node->bounds.y + node->bounds.height > canvas.y;
    if (!intersects) {
      node->problems.push_back("completely off-canvas, not visible");
    }
  }
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

static bool HasProblems(const CheckNode& node) {
  if (!node.problems.empty()) {
    return true;
  }
  for (const auto& child : node.children) {
    if (HasProblems(*child)) {
      return true;
    }
  }
  return false;
}

static std::vector<std::shared_ptr<CheckNode>> FilterProblematicNodes(
    const std::vector<std::shared_ptr<CheckNode>>& nodes) {
  // Find the last node that has problems (directly or in descendants).
  int lastIdx = -1;
  for (int i = static_cast<int>(nodes.size()) - 1; i >= 0; --i) {
    if (HasProblems(*nodes[i])) {
      lastIdx = i;
      break;
    }
  }
  if (lastIdx < 0) {
    return {};
  }
  // Output nodes up to lastIdx: problematic nodes keep details, clean nodes become placeholders.
  std::vector<std::shared_ptr<CheckNode>> result;
  for (int i = 0; i <= lastIdx; ++i) {
    if (HasProblems(*nodes[i])) {
      auto filtered = std::make_shared<CheckNode>(*nodes[i]);
      filtered->children = FilterProblematicNodes(filtered->children);
      if (filtered->children.empty()) {
        filtered->children.clear();
      }
      result.push_back(filtered);
    } else {
      auto placeholder = std::make_shared<CheckNode>();
      placeholder->tagName = nodes[i]->tagName;
      placeholder->placeholder = true;
      result.push_back(placeholder);
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
  if (node.placeholder) {
    os << pad << "<" << node.tagName << "/>\n";
    return;
  }
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
            << "Display layout structure of a PAGX file in XML format. Outputs the full\n"
            << "layout tree with bounds and detected problems for all Layers and elements.\n"
            << "With --problems-only, outputs only nodes with problems and returns non-zero\n"
            << "exit code when issues are found.\n"
            << "\n"
            << "Options:\n"
            << "  --id <id>             Limit scope to the Layer with the specified id\n"
            << "  --xpath <expr>        Limit scope to Layers matched by XPath expression\n"
            << "  --depth <n>           Limit Layer nesting depth (0 or negative = unlimited)\n"
            << "  --problems-only       Only output nodes with problems (non-zero exit if found)\n"
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
    } else if (strcmp(argv[i], "--depth") == 0) {
      if (i + 1 >= argc) {
        std::cerr << "pagx layout: --depth requires an argument\n";
        return 1;
      }
      opts->depth = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--problems-only") == 0) {
      opts->problemsOnly = true;
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
    auto node = BuildLayoutTree(targetLayers[i], originX, originY, i, true, opts.depth);
    if (node) {
      results.push_back(node);
    }
  }

  int problemCount = CountProblems(results);

  // Detect off-canvas layers (only for full-document scope, not scoped queries).
  if (!scopedQuery) {
    DetectOffCanvas(results, document->width, document->height);
    problemCount = CountProblems(results);
  }

  if (opts.problemsOnly) {
    results = FilterProblematicNodes(results);
  }

  // Output XML.
  bool hasTargetScope = !opts.id.empty() || !opts.xpath.empty();
  std::cout << "<layout>\n";
  bool emptyCheck = results.empty() && opts.problemsOnly;
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

  return (opts.problemsOnly && problemCount > 0) ? 1 : 0;
}

}  // namespace pagx::cli
