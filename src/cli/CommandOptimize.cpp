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

#include "cli/CommandOptimize.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "cli/CliUtils.h"
#include "cli/CommandValidator.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/InnerShadowStyle.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
#include "pagx/nodes/SolidColor.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"
#include "renderer/LayerBuilder.h"
#include "renderer/ToTGFX.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/RRect.h"
#include "tgfx/core/Rect.h"

namespace pagx::cli {

struct OptimizeReport {
  int emptyElements = 0;
  int deduplicatedPathData = 0;
  int deduplicatedGradients = 0;
  int mergedGroups = 0;
  int unreferencedResources = 0;
  int pathToRectangle = 0;
  int pathToEllipse = 0;
  int fullCanvasClips = 0;
  int offCanvasLayers = 0;
  int coordinatesLocalized = 0;
  int compositionsExtracted = 0;

  int total() const {
    return emptyElements + deduplicatedPathData + deduplicatedGradients + mergedGroups +
           unreferencedResources + pathToRectangle + pathToEllipse + fullCanvasClips +
           offCanvasLayers + coordinatesLocalized + compositionsExtracted;
  }

  void print() const {
    auto count = total();
    if (count == 0) {
      std::cout << "pagx optimize: no optimizations needed\n";
      return;
    }
    std::cout << "pagx optimize: " << count << " optimizations applied\n";
    if (emptyElements > 0) {
      std::cout << "  - removed " << emptyElements << " empty elements\n";
    }
    if (deduplicatedPathData > 0) {
      std::cout << "  - deduplicated " << deduplicatedPathData << " PathData resources\n";
    }
    if (deduplicatedGradients > 0) {
      std::cout << "  - deduplicated " << deduplicatedGradients << " gradient resources\n";
    }
    if (mergedGroups > 0) {
      std::cout << "  - merged " << mergedGroups << " adjacent groups\n";
    }
    if (unreferencedResources > 0) {
      std::cout << "  - removed " << unreferencedResources << " unreferenced resources\n";
    }
    if (pathToRectangle > 0) {
      std::cout << "  - replaced " << pathToRectangle << " Path with Rectangle\n";
    }
    if (pathToEllipse > 0) {
      std::cout << "  - replaced " << pathToEllipse << " Path with Ellipse\n";
    }
    if (fullCanvasClips > 0) {
      std::cout << "  - removed " << fullCanvasClips << " full-canvas clip masks\n";
    }
    if (offCanvasLayers > 0) {
      std::cout << "  - removed " << offCanvasLayers << " off-canvas layers\n";
    }
    if (coordinatesLocalized > 0) {
      std::cout << "  - localized " << coordinatesLocalized << " layer coordinates\n";
    }
    if (compositionsExtracted > 0) {
      std::cout << "  - extracted " << compositionsExtracted << " layers to compositions\n";
    }
  }
};

static void PrintOptimizeUsage() {
  std::cout << "Usage: pagx optimize [options] <file.pagx>\n"
            << "\n"
            << "Validates, optimizes, and formats a PAGX file in one step.\n"
            << "Validates input against the XSD schema first — aborts with errors if invalid.\n"
            << "Then applies structural optimizations and exports with consistent formatting.\n"
            << "\n"
            << "Optimizations:\n"
            << "  1.  Remove empty elements (empty Layer/Group, zero-width Stroke)\n"
            << "  2.  Deduplicate PathData resources\n"
            << "  3.  Deduplicate gradient resources\n"
            << "  4.  Merge adjacent Groups with identical painters\n"
            << "  5.  Replace Path with Rectangle/Ellipse\n"
            << "  6.  Remove full-canvas clip masks\n"
            << "  7.  Remove off-canvas invisible layers\n"
            << "  8.  Localize layer coordinates\n"
            << "  9.  Extract duplicate layers to compositions\n"
            << "  10. Remove unreferenced resources\n"
            << "\n"
            << "Lint hints (printed but not auto-applied):\n"
            << "  - Child Layers that could be downgraded to Groups\n"
            << "\n"
            << "Options:\n"
            << "  -o, --output <path>   Output file path (default: overwrite input)\n"
            << "  --dry-run             Only print report, do not write output\n"
            << "  -h, --help            Show this help message\n";
}

// --- Helper: Check if any constraint attribute is set ---

static bool HasConstraintAttributes(float left, float right, float top, float bottom, float centerX,
                                    float centerY) {
  return !std::isnan(left) || !std::isnan(right) || !std::isnan(top) || !std::isnan(bottom) ||
         !std::isnan(centerX) || !std::isnan(centerY);
}

// --- Optimization #1: Remove empty elements ---

static bool IsEmptyLayer(const Layer* layer) {
  // Layers with explicit size are intentional (e.g., spacers in container layouts).
  // Preserve them even if visually empty, as they serve as layout placeholders.
  if (!std::isnan(layer->width) || !std::isnan(layer->height)) {
    return false;
  }
  return layer->contents.empty() && layer->children.empty() && layer->composition == nullptr &&
         layer->styles.empty() && layer->filters.empty();
}

static int RemoveEmptyLayers(std::vector<Layer*>& layers, bool insideContainerLayout) {
  auto originalSize = static_cast<int>(layers.size());
  layers.erase(std::remove_if(layers.begin(), layers.end(),
                              [insideContainerLayout](const Layer* layer) {
                                if (insideContainerLayout && layer->includeInLayout) {
                                  return false;
                                }
                                return IsEmptyLayer(layer);
                              }),
               layers.end());
  return originalSize - static_cast<int>(layers.size());
}

static void ClearEmptyMaskReferences(PAGXDocument* document) {
  for (auto& node : document->nodes) {
    if (node->nodeType() != NodeType::Layer) {
      continue;
    }
    auto layer = static_cast<Layer*>(node.get());
    if (layer->mask != nullptr && IsEmptyLayer(layer->mask)) {
      layer->mask = nullptr;
    }
  }
}

static bool IsEmptyElement(const Element* element) {
  if (element->nodeType() == NodeType::Stroke) {
    auto stroke = static_cast<const Stroke*>(element);
    return stroke->width <= 0.0f;
  }
  if (element->nodeType() == NodeType::Group) {
    auto group = static_cast<const Group*>(element);
    return group->elements.empty();
  }
  return false;
}

static int RemoveEmptyElements(std::vector<Element*>& elements) {
  auto originalSize = static_cast<int>(elements.size());
  elements.erase(std::remove_if(elements.begin(), elements.end(), IsEmptyElement), elements.end());
  return originalSize - static_cast<int>(elements.size());
}

static int RemoveEmptyNodes(PAGXDocument* document) {
  int count = 0;
  ClearEmptyMaskReferences(document);
  count += RemoveEmptyLayers(document->layers, false);
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Layer) {
      auto layer = static_cast<Layer*>(node.get());
      count += RemoveEmptyLayers(layer->children, layer->layout != LayoutMode::None);
      count += RemoveEmptyElements(layer->contents);
    } else if (node->nodeType() == NodeType::Group || node->nodeType() == NodeType::TextBox) {
      auto group = static_cast<Group*>(node.get());
      count += RemoveEmptyElements(group->elements);
    } else if (node->nodeType() == NodeType::Composition) {
      auto composition = static_cast<Composition*>(node.get());
      count += RemoveEmptyLayers(composition->layers, false);
    }
  }
  return count;
}

// --- Lint hints: Detect potential optimizations that require human judgment ---

// Forward declaration for ElementsStructurallyEqual
static bool ElementsStructurallyEqual(const Element* a, const Element* b);

struct LintHint {
  std::string layerName = {};
  std::string message = {};
};

// Check if a child Layer uses no Layer-exclusive features and could be a Group.
static bool CanDowngradeToGroup(const Layer* layer) {
  if (!layer->styles.empty() || !layer->filters.empty() || !layer->children.empty()) {
    return false;
  }
  if (layer->mask != nullptr || layer->composition != nullptr) {
    return false;
  }
  if (layer->blendMode != BlendMode::Normal) {
    return false;
  }
  if (layer->hasScrollRect || !layer->visible) {
    return false;
  }
  if (!layer->matrix.isIdentity() || !layer->matrix3D.isIdentity()) {
    return false;
  }
  if (layer->preserve3D || layer->groupOpacity || !layer->passThroughBackground) {
    return false;
  }
  if (!layer->antiAlias) {
    return false;
  }
  if (!layer->id.empty() || !layer->name.empty()) {
    return false;
  }
  if (!layer->customData.empty()) {
    return false;
  }
  // Groups cannot participate in container layout — do not suggest downgrade for layers with
  // layout-related attributes that would be lost.
  if (layer->layout != LayoutMode::None || layer->flex != 0.0f) {
    return false;
  }
  if (!std::isnan(layer->width) || !std::isnan(layer->height)) {
    return false;
  }
  if (HasConstraintAttributes(layer->left, layer->right, layer->top, layer->bottom, layer->centerX,
                              layer->centerY)) {
    return false;
  }
  if (!layer->includeInLayout) {
    return false;
  }
  return true;
}

// Recursively check layer contents for lint issues
static void CollectLintHintsFromContents(const Layer* layer, std::vector<LintHint>& hints,
                                         float repeaterProductSoFar = 1.0f) {
  std::function<void(const std::vector<Element*>&, float)> checkElements =
      [&](const std::vector<Element*>& elements, float outerProduct) {
        // First pass: scan all Repeaters at this level to compute the full product.
        // Repeater affects all sibling elements regardless of order.
        float localProduct = outerProduct;
        for (auto* element : elements) {
          if (element->nodeType() == NodeType::Repeater) {
            auto repeater = static_cast<const Repeater*>(element);
            localProduct *= repeater->copies;
          }
        }

        bool hasRepeater = localProduct > outerProduct;
        bool hasStrokeWithDashes = false;

        // Second pass: check each element with the full product known.
        for (auto* element : elements) {
          NodeType type = element->nodeType();

          // C6: Check Repeater copies
          if (type == NodeType::Repeater) {
            auto repeater = static_cast<const Repeater*>(element);

            // Single Repeater: warn if > 200 copies
            if (repeater->copies > 200.0f) {
              auto name = layer->name.empty() ? layer->id : layer->name;
              hints.push_back({name, "Repeater with high copies count (" +
                                         std::to_string(static_cast<int>(repeater->copies)) +
                                         ") may cause performance issues"});
            }

            // Nested Repeater: warn if total product > 500 and there is an outer Repeater
            if (localProduct > 500.0f && outerProduct > 1.0f) {
              auto name = layer->name.empty() ? layer->id : layer->name;
              hints.push_back({name, "Nested Repeater with product " +
                                         std::to_string(static_cast<int>(localProduct)) +
                                         " (> 500) may cause performance issues"});
            }
          }

          // C8: Check Stroke alignment (only costly under Repeater)
          if (type == NodeType::Stroke) {
            auto stroke = static_cast<const Stroke*>(element);
            if (hasRepeater && stroke->align != StrokeAlign::Center) {
              auto name = layer->name.empty() ? layer->id : layer->name;
              std::string alignStr = (stroke->align == StrokeAlign::Inside) ? "Inside" : "Outside";
              hints.push_back(
                  {name, "Stroke with non-center alignment (" + alignStr +
                             ") in Repeater scope requires expensive CPU path operations"});
            }

            // C9: Check for dashed stroke
            if (!stroke->dashes.empty()) {
              hasStrokeWithDashes = true;
            }
          }

          // C10: Check Path complexity
          if (type == NodeType::Path) {
            auto path = static_cast<const Path*>(element);
            if (path->data != nullptr) {
              auto verbCount = path->data->verbs().size();
              if (verbCount > 15) {
                auto name = layer->name.empty() ? layer->id : layer->name;
                hints.push_back({name, "Path with high complexity (" + std::to_string(verbCount) +
                                           " verbs) may cause rendering performance issues"});
              }
            }
          }

          // Recurse into Group with the full product from this level
          if (type == NodeType::Group || type == NodeType::TextBox) {
            auto group = static_cast<const Group*>(element);
            checkElements(group->elements, localProduct);
          }
        }

        // C9: Check for dashed stroke in Repeater scope
        if (hasRepeater && hasStrokeWithDashes) {
          auto name = layer->name.empty() ? layer->id : layer->name;
          hints.push_back({name,
                           "Dashed Stroke within Repeater scope may cause performance "
                           "issues due to complex rendering calculations"});
        }
      };

  checkElements(layer->contents, repeaterProductSoFar);

  // C7: Check Blur filters for high blur radius
  for (auto* filter : layer->filters) {
    if (filter->nodeType() == NodeType::BlurFilter) {
      auto blur = static_cast<const BlurFilter*>(filter);
      if (blur->blurX > 30.0f || blur->blurY > 30.0f) {
        auto name = layer->name.empty() ? layer->id : layer->name;
        hints.push_back({name, "BlurFilter with high blur radius (X:" +
                                   std::to_string(static_cast<int>(blur->blurX)) +
                                   " Y:" + std::to_string(static_cast<int>(blur->blurY)) +
                                   ") may cause performance issues"});
      }
    }
  }

  // C7: Check Layer styles for high blur radius
  for (auto* style : layer->styles) {
    auto type = style->nodeType();
    if (type == NodeType::DropShadowStyle) {
      auto shadow = static_cast<const DropShadowStyle*>(style);
      if (shadow->blurX > 30.0f || shadow->blurY > 30.0f) {
        auto name = layer->name.empty() ? layer->id : layer->name;
        hints.push_back({name, "DropShadowStyle with high blur radius (X:" +
                                   std::to_string(static_cast<int>(shadow->blurX)) +
                                   " Y:" + std::to_string(static_cast<int>(shadow->blurY)) +
                                   ") may cause performance issues"});
      }
    } else if (type == NodeType::BackgroundBlurStyle) {
      auto bgBlur = static_cast<const BackgroundBlurStyle*>(style);
      if (bgBlur->blurX > 30.0f || bgBlur->blurY > 30.0f) {
        auto name = layer->name.empty() ? layer->id : layer->name;
        hints.push_back({name, "BackgroundBlurStyle with high blur radius (X:" +
                                   std::to_string(static_cast<int>(bgBlur->blurX)) +
                                   " Y:" + std::to_string(static_cast<int>(bgBlur->blurY)) +
                                   ") may cause performance issues"});
      }
    }
  }
}

// Helper to check if layer has high-cost elements (Repeater, Blur, etc.)
static bool HasHighCostElements(const Layer* layer) {
  std::function<bool(const std::vector<Element*>&)> checkElements =
      [&](const std::vector<Element*>& elements) {
        for (auto* element : elements) {
          NodeType type = element->nodeType();
          if (type == NodeType::Repeater) {
            return true;
          }
          if (type == NodeType::Group || type == NodeType::TextBox) {
            auto group = static_cast<const Group*>(element);
            if (checkElements(group->elements)) {
              return true;
            }
          }
        }
        return false;
      };

  // Check for high-cost elements
  if (checkElements(layer->contents)) {
    return true;
  }

  // Check filters for Blur
  for (auto* filter : layer->filters) {
    if (filter->nodeType() == NodeType::BlurFilter) {
      return true;
    }
  }

  // Check styles for Blur and Shadows
  for (auto* style : layer->styles) {
    auto type = style->nodeType();
    if (type == NodeType::DropShadowStyle || type == NodeType::BackgroundBlurStyle ||
        type == NodeType::InnerShadowStyle) {
      return true;
    }
  }

  return false;
}

// Helper to check if mask is a simple solid-fill rectangle (eligible for scrollRect)
static bool IsMaskASimpleRectangle(const Layer* mask) {
  if (mask == nullptr) {
    return false;
  }

  // Mask must have exactly the right structure:
  // - Exactly 2 elements: Rectangle + Fill
  // - Fill must be solid color (not gradient or pattern)
  // - Fill alpha must be 1.0 (opaque)
  // - No other Layer features (matrix, transform, etc.)

  if (mask->contents.size() != 2) {
    return false;
  }

  // Check first element is Rectangle
  if (mask->contents[0]->nodeType() != NodeType::Rectangle) {
    return false;
  }

  // Check second element is Fill
  if (mask->contents[1]->nodeType() != NodeType::Fill) {
    return false;
  }

  auto fill = static_cast<const Fill*>(mask->contents[1]);

  // Fill must be solid color (not gradient or pattern)
  if (fill->color == nullptr || fill->color->nodeType() != NodeType::SolidColor) {
    return false;
  }

  // Fill must be opaque
  if (fill->alpha < 0.999f) {
    return false;
  }

  // Mask layer should have no matrix/transform beyond identity
  if (!mask->matrix.isIdentity() || !mask->matrix3D.isIdentity()) {
    return false;
  }

  // Mask layer should have no styles or filters
  if (!mask->styles.empty() || !mask->filters.empty()) {
    return false;
  }

  // Mask layer should have no blending mode beyond normal
  if (mask->blendMode != BlendMode::Normal) {
    return false;
  }

  return true;
}

static void CollectLintHints(const Layer* layer, std::vector<LintHint>& hints, bool isRoot) {
  // Hint: child Layers that could be downgraded to Groups.
  // Skip when this layer uses container layout — child Layers are layout slots and Groups cannot
  // participate in container layout flow.
  if (!isRoot && !layer->children.empty() && layer->layout == LayoutMode::None) {
    bool allDowngradable = true;
    for (auto* child : layer->children) {
      if (!CanDowngradeToGroup(child)) {
        allDowngradable = false;
        break;
      }
    }
    if (allDowngradable) {
      auto count = static_cast<int>(layer->children.size());
      auto name = layer->name.empty() ? layer->id : layer->name;
      hints.push_back({name, std::to_string(count) + " child Layer(s) use no Layer-exclusive "
                                                     "features and could be downgraded to Groups"});
    }
  }

  // C6-C10: Check contents for various lint issues
  CollectLintHintsFromContents(layer, hints);

  // C11: Check for low opacity with high-cost elements
  if (layer->alpha < 0.2f && layer->alpha > 0.0f && HasHighCostElements(layer)) {
    auto name = layer->name.empty() ? layer->id : layer->name;
    hints.push_back(
        {name,
         "Low opacity (" + std::to_string(static_cast<int>(layer->alpha * 100)) +
             "%) with high-cost elements (Repeater/Blur) may indicate unnecessary rendering"});
  }

  // C13: Check if mask is a simple rectangle (could use scrollRect instead)
  if (layer->mask != nullptr && layer->maskType == MaskType::Alpha &&
      IsMaskASimpleRectangle(layer->mask)) {
    auto name = layer->name.empty() ? layer->id : layer->name;
    hints.push_back(
        {name, "Using rectangular mask could be replaced with scrollRect for better performance"});
  }

  for (auto* child : layer->children) {
    CollectLintHints(child, hints, false);
  }
}

static std::vector<LintHint> CollectLintHints(const PAGXDocument* document) {
  std::vector<LintHint> hints = {};
  for (auto* layer : document->layers) {
    CollectLintHints(layer, hints, true);
  }
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto comp = static_cast<const Composition*>(node.get());
      for (auto* layer : comp->layers) {
        CollectLintHints(layer, hints, true);
      }
    }
  }
  return hints;
}

static void PrintLintHints(const std::vector<LintHint>& hints) {
  if (hints.empty()) {
    return;
  }
  std::cout << "pagx optimize: " << hints.size() << " lint hint(s)\n";
  for (auto& hint : hints) {
    std::cout << "  - ";
    if (!hint.layerName.empty()) {
      std::cout << "Layer \"" << hint.layerName << "\": ";
    }
    std::cout << hint.message << "\n";
  }
}

// --- Optimization #2: Deduplicate PathData ---

static bool PathDataEqual(const PathData* a, const PathData* b) {
  if (a == b) {
    return true;
  }
  if (a == nullptr || b == nullptr) {
    return false;
  }
  return a->verbs() == b->verbs() && a->points() == b->points();
}

static int DeduplicatePathData(PAGXDocument* document) {
  std::vector<PathData*> uniquePaths = {};
  std::unordered_map<const PathData*, PathData*> mergeMap = {};

  for (auto& node : document->nodes) {
    if (node->nodeType() != NodeType::PathData) {
      continue;
    }
    auto pathData = static_cast<PathData*>(node.get());
    bool found = false;
    for (auto* existing : uniquePaths) {
      if (PathDataEqual(existing, pathData)) {
        mergeMap[pathData] = existing;
        found = true;
        break;
      }
    }
    if (!found) {
      uniquePaths.push_back(pathData);
    }
  }

  if (mergeMap.empty()) {
    return 0;
  }

  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Path) {
      auto path = static_cast<Path*>(node.get());
      auto it = mergeMap.find(path->data);
      if (it != mergeMap.end()) {
        path->data = it->second;
      }
    } else if (node->nodeType() == NodeType::TextPath) {
      auto textPath = static_cast<TextPath*>(node.get());
      auto it = mergeMap.find(textPath->path);
      if (it != mergeMap.end()) {
        textPath->path = it->second;
      }
    } else if (node->nodeType() == NodeType::Glyph) {
      auto glyph = static_cast<Glyph*>(node.get());
      auto it = mergeMap.find(glyph->path);
      if (it != mergeMap.end()) {
        glyph->path = it->second;
      }
    }
  }

  return static_cast<int>(mergeMap.size());
}

// --- Optimization #3: Deduplicate gradients ---

static bool ColorStopsEqual(const std::vector<ColorStop*>& a, const std::vector<ColorStop*>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (size_t i = 0; i < a.size(); i++) {
    if (a[i]->offset != b[i]->offset || a[i]->color != b[i]->color) {
      return false;
    }
  }
  return true;
}

static bool GradientEqual(const ColorSource* a, const ColorSource* b) {
  if (a->nodeType() != b->nodeType()) {
    return false;
  }
  auto type = a->nodeType();
  if (type == NodeType::LinearGradient) {
    auto ga = static_cast<const LinearGradient*>(a);
    auto gb = static_cast<const LinearGradient*>(b);
    return ga->startPoint == gb->startPoint && ga->endPoint == gb->endPoint &&
           ga->matrix == gb->matrix && ColorStopsEqual(ga->colorStops, gb->colorStops);
  }
  if (type == NodeType::RadialGradient) {
    auto ga = static_cast<const RadialGradient*>(a);
    auto gb = static_cast<const RadialGradient*>(b);
    return ga->center == gb->center && ga->radius == gb->radius && ga->matrix == gb->matrix &&
           ColorStopsEqual(ga->colorStops, gb->colorStops);
  }
  if (type == NodeType::ConicGradient) {
    auto ga = static_cast<const ConicGradient*>(a);
    auto gb = static_cast<const ConicGradient*>(b);
    return ga->center == gb->center && ga->startAngle == gb->startAngle &&
           ga->endAngle == gb->endAngle && ga->matrix == gb->matrix &&
           ColorStopsEqual(ga->colorStops, gb->colorStops);
  }
  if (type == NodeType::DiamondGradient) {
    auto ga = static_cast<const DiamondGradient*>(a);
    auto gb = static_cast<const DiamondGradient*>(b);
    return ga->center == gb->center && ga->radius == gb->radius && ga->matrix == gb->matrix &&
           ColorStopsEqual(ga->colorStops, gb->colorStops);
  }
  return false;
}

static bool IsGradient(NodeType type) {
  return type == NodeType::LinearGradient || type == NodeType::RadialGradient ||
         type == NodeType::ConicGradient || type == NodeType::DiamondGradient;
}

static int DeduplicateColorSources(PAGXDocument* document) {
  std::vector<ColorSource*> uniqueSources = {};
  std::unordered_map<const ColorSource*, ColorSource*> mergeMap = {};

  for (auto& node : document->nodes) {
    if (!IsGradient(node->nodeType())) {
      continue;
    }
    auto source = static_cast<ColorSource*>(node.get());
    bool found = false;
    for (auto* existing : uniqueSources) {
      if (GradientEqual(existing, source)) {
        mergeMap[source] = existing;
        found = true;
        break;
      }
    }
    if (!found) {
      uniqueSources.push_back(source);
    }
  }

  if (mergeMap.empty()) {
    return 0;
  }

  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Fill) {
      auto fill = static_cast<Fill*>(node.get());
      auto it = mergeMap.find(fill->color);
      if (it != mergeMap.end()) {
        fill->color = it->second;
      }
    } else if (node->nodeType() == NodeType::Stroke) {
      auto stroke = static_cast<Stroke*>(node.get());
      auto it = mergeMap.find(stroke->color);
      if (it != mergeMap.end()) {
        stroke->color = it->second;
      }
    }
  }

  return static_cast<int>(mergeMap.size());
}

// --- Optimization #4: Remove unreferenced resources ---

static void CollectReferencedNodes(const std::vector<Element*>& elements,
                                   std::unordered_set<const Node*>& referenced);

static void CollectColorStops(const ColorSource* source,
                              std::unordered_set<const Node*>& referenced) {
  const std::vector<ColorStop*>* stops = nullptr;
  auto type = source->nodeType();
  if (type == NodeType::LinearGradient) {
    stops = &static_cast<const LinearGradient*>(source)->colorStops;
  } else if (type == NodeType::RadialGradient) {
    stops = &static_cast<const RadialGradient*>(source)->colorStops;
  } else if (type == NodeType::ConicGradient) {
    stops = &static_cast<const ConicGradient*>(source)->colorStops;
  } else if (type == NodeType::DiamondGradient) {
    stops = &static_cast<const DiamondGradient*>(source)->colorStops;
  }
  if (stops != nullptr) {
    for (auto* stop : *stops) {
      referenced.insert(stop);
    }
  }
}

static void CollectReferencedNodesFromColorSource(const ColorSource* source,
                                                  std::unordered_set<const Node*>& referenced) {
  if (source == nullptr) {
    return;
  }
  referenced.insert(source);
  if (IsGradient(source->nodeType())) {
    CollectColorStops(source, referenced);
  } else if (source->nodeType() == NodeType::ImagePattern) {
    auto pattern = static_cast<const ImagePattern*>(source);
    if (pattern->image != nullptr) {
      referenced.insert(pattern->image);
    }
  }
}

static void CollectReferencedNodes(const std::vector<Element*>& elements,
                                   std::unordered_set<const Node*>& referenced) {
  for (auto* element : elements) {
    if (element == nullptr) {
      continue;
    }
    referenced.insert(element);
    auto type = element->nodeType();
    if (type == NodeType::Fill) {
      auto fill = static_cast<const Fill*>(element);
      CollectReferencedNodesFromColorSource(fill->color, referenced);
    } else if (type == NodeType::Stroke) {
      auto stroke = static_cast<const Stroke*>(element);
      CollectReferencedNodesFromColorSource(stroke->color, referenced);
    } else if (type == NodeType::Path) {
      auto path = static_cast<const Path*>(element);
      if (path->data != nullptr) {
        referenced.insert(path->data);
      }
    } else if (type == NodeType::Text) {
      auto text = static_cast<const Text*>(element);
      for (auto* run : text->glyphRuns) {
        referenced.insert(run);
        if (run->font != nullptr) {
          referenced.insert(run->font);
          for (auto* glyph : run->font->glyphs) {
            referenced.insert(glyph);
            if (glyph->path != nullptr) {
              referenced.insert(glyph->path);
            }
            if (glyph->image != nullptr) {
              referenced.insert(glyph->image);
            }
          }
        }
      }
    } else if (type == NodeType::Group || type == NodeType::TextBox) {
      auto group = static_cast<const Group*>(element);
      CollectReferencedNodes(group->elements, referenced);
    } else if (type == NodeType::TextPath) {
      auto textPath = static_cast<const TextPath*>(element);
      if (textPath->path != nullptr) {
        referenced.insert(textPath->path);
      }
    } else if (type == NodeType::TextModifier) {
      auto modifier = static_cast<const TextModifier*>(element);
      for (auto* selector : modifier->selectors) {
        referenced.insert(selector);
      }
    }
  }
}

static void CollectReferencedNodesFromLayer(const Layer* layer,
                                            std::unordered_set<const Node*>& referenced) {
  if (layer == nullptr) {
    return;
  }
  referenced.insert(layer);
  CollectReferencedNodes(layer->contents, referenced);
  for (auto* style : layer->styles) {
    referenced.insert(style);
  }
  for (auto* filter : layer->filters) {
    referenced.insert(filter);
  }
  if (layer->mask != nullptr) {
    CollectReferencedNodesFromLayer(layer->mask, referenced);
  }
  if (layer->composition != nullptr) {
    referenced.insert(layer->composition);
    for (auto* child : layer->composition->layers) {
      CollectReferencedNodesFromLayer(child, referenced);
    }
  }
  for (auto* child : layer->children) {
    CollectReferencedNodesFromLayer(child, referenced);
  }
}

static int RemoveUnreferencedResources(PAGXDocument* document) {
  std::unordered_set<const Node*> referenced = {};
  for (auto* layer : document->layers) {
    CollectReferencedNodesFromLayer(layer, referenced);
  }
  auto& nodes = document->nodes;
  auto originalSize = nodes.size();
  size_t writeIndex = 0;
  for (size_t i = 0; i < nodes.size(); i++) {
    if (referenced.find(nodes[i].get()) != referenced.end()) {
      if (writeIndex != i) {
        nodes[writeIndex] = std::move(nodes[i]);
      }
      writeIndex++;
    }
  }
  nodes.resize(writeIndex);
  return static_cast<int>(originalSize - writeIndex);
}

// --- Optimization #4b: Merge adjacent Groups ---

static bool IsGeometry(NodeType type) {
  return type == NodeType::Rectangle || type == NodeType::Ellipse || type == NodeType::Polystar ||
         type == NodeType::Path;
}

static bool IsPainter(NodeType type) {
  return type == NodeType::Fill || type == NodeType::Stroke;
}

static bool IsModifier(NodeType type) {
  return type == NodeType::TrimPath || type == NodeType::RoundCorner || type == NodeType::MergePath;
}

static bool HasDefaultGroupTransform(const Group* group) {
  if (group->anchor.x != 0 || group->anchor.y != 0 || group->position.x != 0 ||
      group->position.y != 0 || group->rotation != 0 || group->scale.x != 1 ||
      group->scale.y != 1 || group->skew != 0 || group->skewAxis != 0 || group->alpha != 1) {
    return false;
  }
  // Groups with constraint attributes or layout size cannot be merged — each Group serves as an
  // independent constraint reference frame and merging would lose positioning semantics.
  if (HasConstraintAttributes(group->left, group->right, group->top, group->bottom, group->centerX,
                              group->centerY)) {
    return false;
  }
  if (!std::isnan(group->width) || !std::isnan(group->height)) {
    return false;
  }
  return true;
}

// Split a Group's elements into geometry and painters. Returns false if the Group contains
// modifiers, nested Groups, or other incompatible elements.
static bool SplitGeometryAndPainters(const Group* group, std::vector<Element*>& geometry,
                                     std::vector<Element*>& painters) {
  bool seenPainter = false;
  for (auto* element : group->elements) {
    auto type = element->nodeType();
    if (IsModifier(type)) {
      return false;
    }
    if (type == NodeType::Group || type == NodeType::Repeater || type == NodeType::Text ||
        type == NodeType::TextBox || type == NodeType::TextPath || type == NodeType::TextModifier) {
      return false;
    }
    if (IsPainter(type)) {
      seenPainter = true;
      painters.push_back(element);
    } else if (IsGeometry(type)) {
      if (seenPainter) {
        return false;
      }
      geometry.push_back(element);
    } else {
      return false;
    }
  }
  return !geometry.empty() && !painters.empty();
}

static bool PaintersEqual(const std::vector<Element*>& a, const std::vector<Element*>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (size_t i = 0; i < a.size(); i++) {
    if (!ElementsStructurallyEqual(a[i], b[i])) {
      return false;
    }
  }
  return true;
}

static void AppendPathData(const PathData* source, PathData* target) {
  source->forEach([target](PathVerb verb, const Point* pts) {
    switch (verb) {
      case PathVerb::Move:
        target->moveTo(pts[0].x, pts[0].y);
        break;
      case PathVerb::Line:
        target->lineTo(pts[0].x, pts[0].y);
        break;
      case PathVerb::Quad:
        target->quadTo(pts[0].x, pts[0].y, pts[1].x, pts[1].y);
        break;
      case PathVerb::Cubic:
        target->cubicTo(pts[0].x, pts[0].y, pts[1].x, pts[1].y, pts[2].x, pts[2].y);
        break;
      case PathVerb::Close:
        target->close();
        break;
    }
  });
}

static int MergeAdjacentGroupsInElements(PAGXDocument* document, std::vector<Element*>& elements) {
  int count = 0;
  // Recursively process nested Groups first.
  for (auto* element : elements) {
    if (element->nodeType() == NodeType::Group || element->nodeType() == NodeType::TextBox) {
      auto group = static_cast<Group*>(element);
      count += MergeAdjacentGroupsInElements(document, group->elements);
    }
  }
  // Scan for adjacent mergeable Groups.
  size_t i = 0;
  while (i + 1 < elements.size()) {
    if (elements[i]->nodeType() != NodeType::Group ||
        elements[i + 1]->nodeType() != NodeType::Group) {
      i++;
      continue;
    }
    auto groupA = static_cast<Group*>(elements[i]);
    auto groupB = static_cast<Group*>(elements[i + 1]);
    if (!HasDefaultGroupTransform(groupA) || !HasDefaultGroupTransform(groupB)) {
      i++;
      continue;
    }
    std::vector<Element*> geoA = {};
    std::vector<Element*> paintA = {};
    std::vector<Element*> geoB = {};
    std::vector<Element*> paintB = {};
    if (!SplitGeometryAndPainters(groupA, geoA, paintA) ||
        !SplitGeometryAndPainters(groupB, geoB, paintB)) {
      i++;
      continue;
    }
    if (!PaintersEqual(paintA, paintB)) {
      i++;
      continue;
    }
    // Check if all geometry in both groups are Paths (multi-M merge).
    bool allPaths = true;
    for (auto* geo : geoA) {
      if (geo->nodeType() != NodeType::Path) {
        allPaths = false;
        break;
      }
    }
    if (allPaths) {
      for (auto* geo : geoB) {
        if (geo->nodeType() != NodeType::Path) {
          allPaths = false;
          break;
        }
      }
    }
    if (allPaths) {
      // Path multi-M merge: concatenate all PathData into the first Path.
      auto targetPath = static_cast<Path*>(geoA[0]);
      auto mergedData = document->makeNode<PathData>();
      if (targetPath->data != nullptr) {
        AppendPathData(targetPath->data, mergedData);
      }
      for (size_t j = 1; j < geoA.size(); j++) {
        auto srcPath = static_cast<Path*>(geoA[j]);
        if (srcPath->data != nullptr) {
          AppendPathData(srcPath->data, mergedData);
        }
      }
      for (auto* geo : geoB) {
        auto srcPath = static_cast<Path*>(geo);
        if (srcPath->data != nullptr) {
          AppendPathData(srcPath->data, mergedData);
        }
      }
      targetPath->data = mergedData;
      // Remove extra Path elements from groupA, keep only the first.
      std::vector<Element*> newElements = {};
      newElements.push_back(geoA[0]);
      newElements.insert(newElements.end(), paintA.begin(), paintA.end());
      groupA->elements = newElements;
    } else {
      // Shape merge: move geometry from groupB before groupA's painters.
      std::vector<Element*> newElements = {};
      newElements.insert(newElements.end(), geoA.begin(), geoA.end());
      newElements.insert(newElements.end(), geoB.begin(), geoB.end());
      newElements.insert(newElements.end(), paintA.begin(), paintA.end());
      groupA->elements = newElements;
    }
    // Remove groupB from the elements list.
    elements.erase(elements.begin() + static_cast<ptrdiff_t>(i) + 1);
    count++;
    // Don't advance i — re-check the merged group against the next element.
  }
  return count;
}

static int MergeAdjacentGroups(PAGXDocument* document) {
  int count = 0;
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Layer) {
      auto layer = static_cast<Layer*>(node.get());
      count += MergeAdjacentGroupsInElements(document, layer->contents);
    } else if (node->nodeType() == NodeType::Group || node->nodeType() == NodeType::TextBox) {
      auto group = static_cast<Group*>(node.get());
      count += MergeAdjacentGroupsInElements(document, group->elements);
    }
  }
  return count;
}

// --- Optimization #5: Path → Rectangle / Ellipse ---

enum class PrimitiveType { Rect, RoundRect, Oval };

struct PrimitiveReplacement {
  std::vector<Element*>* elements = nullptr;
  size_t index = 0;
  PrimitiveType type = PrimitiveType::Rect;
  tgfx::Rect bounds = {};
  float radiusX = 0.0f;
  bool reversed = false;
};

static void ReplacePathsWithPrimitives(PAGXDocument* document, int& rectCount, int& ellipseCount) {
  // Collect replacements first to avoid modifying document->nodes during iteration.
  // document->makeNode() may trigger vector reallocation that invalidates iterators.
  std::vector<PrimitiveReplacement> replacements = {};
  for (auto& node : document->nodes) {
    std::vector<Element*>* elements = nullptr;
    if (node->nodeType() == NodeType::Layer) {
      elements = &static_cast<Layer*>(node.get())->contents;
    } else if (node->nodeType() == NodeType::Group || node->nodeType() == NodeType::TextBox) {
      elements = &static_cast<Group*>(node.get())->elements;
    }
    if (elements == nullptr) {
      continue;
    }
    for (size_t i = 0; i < elements->size(); i++) {
      auto element = (*elements)[i];
      if (element->nodeType() != NodeType::Path) {
        continue;
      }
      auto path = static_cast<Path*>(element);
      if (path->data == nullptr) {
        continue;
      }
      auto tgfxPath = ToTGFX(*path->data);
      tgfx::RRect rrect = {};
      tgfx::Rect rect = {};
      PrimitiveReplacement rep = {};
      if (tgfxPath.isRRect(&rrect)) {
        rep.bounds = rrect.rect;
        rep.radiusX = rrect.radii.x;
        rep.type = rrect.isOval() ? PrimitiveType::Oval : PrimitiveType::RoundRect;
      } else if (tgfxPath.isOval(&rect)) {
        rep.bounds = rect;
        rep.type = PrimitiveType::Oval;
      } else if (tgfxPath.isRect(&rect)) {
        rep.bounds = rect;
        rep.type = PrimitiveType::Rect;
      } else {
        continue;
      }
      rep.elements = elements;
      rep.index = i;
      rep.reversed = path->reversed;
      replacements.push_back(rep);
    }
  }

  // Apply replacements after iteration is complete.
  for (auto& rep : replacements) {
    auto width = rep.bounds.width();
    auto height = rep.bounds.height();
    auto centerX = rep.bounds.left + width * 0.5f;
    auto centerY = rep.bounds.top + height * 0.5f;
    if (rep.type == PrimitiveType::Oval) {
      auto ellipse = document->makeNode<Ellipse>();
      ellipse->position = {centerX, centerY};
      ellipse->size = {width, height};
      ellipse->reversed = rep.reversed;
      (*rep.elements)[rep.index] = ellipse;
      ellipseCount++;
    } else {
      auto rectangle = document->makeNode<Rectangle>();
      rectangle->position = {centerX, centerY};
      rectangle->size = {width, height};
      if (rep.type == PrimitiveType::RoundRect) {
        auto minSide = std::min(width, height);
        rectangle->roundness = minSide > 0 ? rep.radiusX / (minSide * 0.5f) * 100.0f : 0.0f;
      }
      rectangle->reversed = rep.reversed;
      (*rep.elements)[rep.index] = rectangle;
      rectCount++;
    }
  }
}

// --- Optimization #6: Remove full-canvas clip masks ---

static void CollectMaskReferences(const std::vector<Layer*>& layers,
                                  std::unordered_set<const Layer*>& maskedLayers) {
  for (auto* layer : layers) {
    if (layer->mask != nullptr) {
      maskedLayers.insert(layer->mask);
    }
    if (layer->composition != nullptr) {
      CollectMaskReferences(layer->composition->layers, maskedLayers);
    }
    CollectMaskReferences(layer->children, maskedLayers);
  }
}

static bool IsFullCanvasClipMask(const Layer* maskLayer, float canvasWidth, float canvasHeight) {
  if (!maskLayer->visible) {
    return false;
  }
  if (maskLayer->x != 0 || maskLayer->y != 0) {
    return false;
  }
  if (!maskLayer->matrix.isIdentity()) {
    return false;
  }
  if (maskLayer->contents.size() != 1) {
    return false;
  }
  auto element = maskLayer->contents[0];
  if (element->nodeType() != NodeType::Rectangle) {
    return false;
  }
  auto rect = static_cast<const Rectangle*>(element);
  auto left = rect->position.x - rect->size.width * 0.5f;
  auto top = rect->position.y - rect->size.height * 0.5f;
  return left <= 0 && top <= 0 && rect->size.width >= canvasWidth &&
         rect->size.height >= canvasHeight;
}

static int RemoveFullCanvasClipMasks(PAGXDocument* document, const std::vector<Layer*>& layers) {
  int count = 0;
  for (auto* layer : layers) {
    if (layer->mask != nullptr &&
        IsFullCanvasClipMask(layer->mask, document->width, document->height)) {
      layer->mask = nullptr;
      count++;
    }
    if (layer->composition != nullptr) {
      count += RemoveFullCanvasClipMasks(document, layer->composition->layers);
    }
    count += RemoveFullCanvasClipMasks(document, layer->children);
  }
  return count;
}

// --- Optimization #7: Remove off-canvas layers ---

static int RemoveOffCanvasChildren(
    const std::unordered_map<const Layer*, std::shared_ptr<tgfx::Layer>>& layerMap,
    const std::unordered_set<const Layer*>& maskedLayers, const tgfx::Rect& canvasRect,
    tgfx::Layer* rootLayer, std::vector<Layer*>& layers, bool insideContainerLayout) {
  int count = 0;
  size_t writeIndex = 0;
  for (size_t i = 0; i < layers.size(); i++) {
    auto* pagxLayer = layers[i];
    bool shouldRemove = false;
    // Skip removal for layers participating in a container layout flow.
    if (!(insideContainerLayout && pagxLayer->includeInLayout)) {
      if (pagxLayer->visible && maskedLayers.find(pagxLayer) == maskedLayers.end() &&
          pagxLayer->styles.empty() && pagxLayer->filters.empty()) {
        auto it = layerMap.find(pagxLayer);
        if (it != layerMap.end()) {
          auto bounds = it->second->getBounds(rootLayer, true);
          if (!tgfx::Rect::Intersects(bounds, canvasRect)) {
            shouldRemove = true;
          }
        }
      }
    }
    if (shouldRemove) {
      count++;
    } else {
      bool childInContainer = pagxLayer->layout != LayoutMode::None;
      count += RemoveOffCanvasChildren(layerMap, maskedLayers, canvasRect, rootLayer,
                                       pagxLayer->children, childInContainer);
      layers[writeIndex++] = layers[i];
    }
  }
  layers.resize(writeIndex);
  return count;
}

static int RemoveOffCanvasLayers(PAGXDocument* document) {
  FontConfig fontProvider = {};
  SetupSystemFallbackFonts(fontProvider);
  auto result = LayerBuilder::BuildWithMap(document, &fontProvider);
  if (result.root == nullptr) {
    return 0;
  }
  auto canvasRect = tgfx::Rect::MakeWH(document->width, document->height);
  std::unordered_set<const Layer*> maskedLayers = {};
  CollectMaskReferences(document->layers, maskedLayers);
  return RemoveOffCanvasChildren(result.layerMap, maskedLayers, canvasRect, result.root.get(),
                                 document->layers, false);
}

// --- Optimization #8: Localize coordinates ---

static bool ShouldSkipLocalization(const Layer* layer) {
  if (!layer->matrix.isIdentity()) {
    return true;
  }
  if (layer->composition != nullptr) {
    return true;
  }
  if (layer->contents.empty()) {
    return true;
  }
  // Skip localization when the Layer itself uses constraint attributes, as constraints override
  // x/y at layout time — modifying x/y via localization would be overwritten, leaving the
  // internal coordinate offsets incorrect.
  if (HasConstraintAttributes(layer->left, layer->right, layer->top, layer->bottom, layer->centerX,
                              layer->centerY)) {
    return true;
  }
  // Skip localization for layers with constraint-based elements, as constraints imply
  // precise layout control that should be preserved.
  for (auto* element : layer->contents) {
    auto type = element->nodeType();
    if (type == NodeType::Rectangle) {
      auto rect = static_cast<const Rectangle*>(element);
      if (HasConstraintAttributes(rect->left, rect->right, rect->top, rect->bottom, rect->centerX,
                                  rect->centerY)) {
        return true;
      }
    } else if (type == NodeType::Ellipse) {
      auto ellipse = static_cast<const Ellipse*>(element);
      if (HasConstraintAttributes(ellipse->left, ellipse->right, ellipse->top, ellipse->bottom,
                                  ellipse->centerX, ellipse->centerY)) {
        return true;
      }
    } else if (type == NodeType::Polystar) {
      auto polystar = static_cast<const Polystar*>(element);
      if (HasConstraintAttributes(polystar->left, polystar->right, polystar->top, polystar->bottom,
                                  polystar->centerX, polystar->centerY)) {
        return true;
      }
    } else if (type == NodeType::Path) {
      auto path = static_cast<const Path*>(element);
      if (HasConstraintAttributes(path->left, path->right, path->top, path->bottom, path->centerX,
                                  path->centerY)) {
        return true;
      }
    } else if (type == NodeType::Text) {
      auto text = static_cast<const Text*>(element);
      if (HasConstraintAttributes(text->left, text->right, text->top, text->bottom, text->centerX,
                                  text->centerY)) {
        return true;
      }
    } else if (type == NodeType::Group || type == NodeType::TextBox) {
      auto group = static_cast<const Group*>(element);
      if (HasConstraintAttributes(group->left, group->right, group->top, group->bottom,
                                  group->centerX, group->centerY)) {
        return true;
      }
    }
  }
  return false;
}

// Compute the maximum stroke expansion from contents. For center-aligned strokes, the expansion is
// half the stroke width. For outer-aligned strokes, it is the full width.
static float ComputeStrokeExpansion(const std::vector<Element*>& contents) {
  float expansion = 0.0f;
  for (auto* element : contents) {
    if (element->nodeType() == NodeType::Stroke) {
      auto stroke = static_cast<const Stroke*>(element);
      float strokeExpansion = 0.0f;
      if (stroke->align == StrokeAlign::Center) {
        strokeExpansion = stroke->width * 0.5f;
      } else if (stroke->align == StrokeAlign::Outside) {
        strokeExpansion = stroke->width;
      }
      expansion = std::max(expansion, strokeExpansion);
    }
  }
  return expansion;
}

// Compute the bounding box of shape elements including stroke expansion.
static bool ComputeShapeBounds(const std::vector<Element*>& contents, float& minX, float& minY,
                               float& maxX, float& maxY) {
  minX = FLT_MAX;
  minY = FLT_MAX;
  maxX = -FLT_MAX;
  maxY = -FLT_MAX;
  bool hasGeometry = false;

  for (auto* element : contents) {
    auto type = element->nodeType();
    if (type == NodeType::Rectangle) {
      auto rect = static_cast<const Rectangle*>(element);
      float halfW = rect->size.width * 0.5f;
      float halfH = rect->size.height * 0.5f;
      minX = std::min(minX, rect->position.x - halfW);
      minY = std::min(minY, rect->position.y - halfH);
      maxX = std::max(maxX, rect->position.x + halfW);
      maxY = std::max(maxY, rect->position.y + halfH);
      hasGeometry = true;
    } else if (type == NodeType::Ellipse) {
      auto ellipse = static_cast<const Ellipse*>(element);
      float halfW = ellipse->size.width * 0.5f;
      float halfH = ellipse->size.height * 0.5f;
      minX = std::min(minX, ellipse->position.x - halfW);
      minY = std::min(minY, ellipse->position.y - halfH);
      maxX = std::max(maxX, ellipse->position.x + halfW);
      maxY = std::max(maxY, ellipse->position.y + halfH);
      hasGeometry = true;
    } else if (type == NodeType::Polystar) {
      auto polystar = static_cast<const Polystar*>(element);
      float r = polystar->outerRadius;
      minX = std::min(minX, polystar->position.x - r);
      minY = std::min(minY, polystar->position.y - r);
      maxX = std::max(maxX, polystar->position.x + r);
      maxY = std::max(maxY, polystar->position.y + r);
      hasGeometry = true;
    } else if (type == NodeType::Path) {
      auto path = static_cast<const Path*>(element);
      if (path->data != nullptr && !path->data->isEmpty()) {
        auto bounds = path->data->getBounds();
        minX = std::min(minX, bounds.x);
        minY = std::min(minY, bounds.y);
        maxX = std::max(maxX, bounds.x + bounds.width);
        maxY = std::max(maxY, bounds.y + bounds.height);
        hasGeometry = true;
      }
    }
  }
  if (hasGeometry) {
    auto expansion = ComputeStrokeExpansion(contents);
    minX -= expansion;
    minY -= expansion;
    maxX += expansion;
    maxY += expansion;
  }
  return hasGeometry;
}

static void ComputeLocalizationOffset(const std::vector<Element*>& contents, float& offsetX,
                                      float& offsetY) {
  // Check if there is a TextBox that controls layout.
  for (auto* element : contents) {
    if (element->nodeType() == NodeType::TextBox) {
      auto textBox = static_cast<const TextBox*>(element);
      offsetX = textBox->position.x;
      offsetY = textBox->position.y;
      return;
    }
  }

  // No TextBox - compute bounding box center of geometry and position elements.
  float minX = FLT_MAX;
  float minY = FLT_MAX;
  float maxX = -FLT_MAX;
  float maxY = -FLT_MAX;
  bool hasGeometry = ComputeShapeBounds(contents, minX, minY, maxX, maxY);

  for (auto* element : contents) {
    auto type = element->nodeType();
    if (type == NodeType::Text) {
      auto text = static_cast<const Text*>(element);
      minX = std::min(minX, text->position.x);
      minY = std::min(minY, text->position.y);
      maxX = std::max(maxX, text->position.x);
      maxY = std::max(maxY, text->position.y);
      hasGeometry = true;
    } else if (type == NodeType::Group) {
      auto group = static_cast<const Group*>(element);
      minX = std::min(minX, group->position.x);
      minY = std::min(minY, group->position.y);
      maxX = std::max(maxX, group->position.x);
      maxY = std::max(maxY, group->position.y);
      hasGeometry = true;
    }
  }

  if (hasGeometry) {
    // Use bounding box top-left as offset, not center. This ensures that after
    // localization, element positions can match default values and be skipped during
    // export, preserving rendering consistency through the optimize-export-import cycle.
    offsetX = minX;
    offsetY = minY;
  }
}

static void OffsetPathData(PathData* source, PathData* target, float offsetX, float offsetY) {
  const auto& verbs = source->verbs();
  const auto& points = source->points();
  size_t pointIndex = 0;
  for (auto verb : verbs) {
    const Point* pts = points.data() + pointIndex;
    switch (verb) {
      case PathVerb::Move:
        target->moveTo(pts[0].x - offsetX, pts[0].y - offsetY);
        break;
      case PathVerb::Line:
        target->lineTo(pts[0].x - offsetX, pts[0].y - offsetY);
        break;
      case PathVerb::Quad:
        target->quadTo(pts[0].x - offsetX, pts[0].y - offsetY, pts[1].x - offsetX,
                       pts[1].y - offsetY);
        break;
      case PathVerb::Cubic:
        target->cubicTo(pts[0].x - offsetX, pts[0].y - offsetY, pts[1].x - offsetX,
                        pts[1].y - offsetY, pts[2].x - offsetX, pts[2].y - offsetY);
        break;
      case PathVerb::Close:
        target->close();
        break;
    }
    pointIndex += PathData::PointsPerVerb(verb);
  }
}

static ColorSource* OffsetColorSource(
    PAGXDocument* document, ColorSource* source, float offsetX, float offsetY,
    std::unordered_map<const ColorSource*, ColorSource*>& offsetMap) {
  if (source == nullptr) {
    return nullptr;
  }
  auto type = source->nodeType();
  if (type == NodeType::SolidColor || type == NodeType::ImagePattern) {
    return source;
  }
  auto it = offsetMap.find(source);
  if (it != offsetMap.end()) {
    return it->second;
  }
  ColorSource* result = nullptr;
  if (type == NodeType::LinearGradient) {
    auto* original = static_cast<LinearGradient*>(source);
    auto* grad = document->makeNode<LinearGradient>();
    grad->startPoint = {original->startPoint.x - offsetX, original->startPoint.y - offsetY};
    grad->endPoint = {original->endPoint.x - offsetX, original->endPoint.y - offsetY};
    grad->matrix = original->matrix;
    grad->colorStops = original->colorStops;
    result = grad;
  } else if (type == NodeType::RadialGradient) {
    auto* original = static_cast<RadialGradient*>(source);
    auto* grad = document->makeNode<RadialGradient>();
    grad->center = {original->center.x - offsetX, original->center.y - offsetY};
    grad->radius = original->radius;
    grad->matrix = original->matrix;
    grad->colorStops = original->colorStops;
    result = grad;
  } else if (type == NodeType::ConicGradient) {
    auto* original = static_cast<ConicGradient*>(source);
    auto* grad = document->makeNode<ConicGradient>();
    grad->center = {original->center.x - offsetX, original->center.y - offsetY};
    grad->startAngle = original->startAngle;
    grad->endAngle = original->endAngle;
    grad->matrix = original->matrix;
    grad->colorStops = original->colorStops;
    result = grad;
  } else if (type == NodeType::DiamondGradient) {
    auto* original = static_cast<DiamondGradient*>(source);
    auto* grad = document->makeNode<DiamondGradient>();
    grad->center = {original->center.x - offsetX, original->center.y - offsetY};
    grad->radius = original->radius;
    grad->matrix = original->matrix;
    grad->colorStops = original->colorStops;
    result = grad;
  }
  if (result != nullptr) {
    offsetMap[source] = result;
  }
  return result != nullptr ? result : source;
}

static void ApplyLocalizationToElements(PAGXDocument* document,
                                        const std::vector<Element*>& contents, float offsetX,
                                        float offsetY) {
  // Track already-offset PathData to avoid duplicating shared instances within the same layer.
  std::unordered_map<const PathData*, PathData*> offsetPathMap = {};
  for (auto* element : contents) {
    auto type = element->nodeType();
    if (type == NodeType::Rectangle) {
      auto rect = static_cast<Rectangle*>(element);
      rect->position.x -= offsetX;
      rect->position.y -= offsetY;
      if (!std::isnan(rect->left)) {
        rect->left -= offsetX;
      }
      if (!std::isnan(rect->top)) {
        rect->top -= offsetY;
      }
    } else if (type == NodeType::Ellipse) {
      auto ellipse = static_cast<Ellipse*>(element);
      ellipse->position.x -= offsetX;
      ellipse->position.y -= offsetY;
      if (!std::isnan(ellipse->left)) {
        ellipse->left -= offsetX;
      }
      if (!std::isnan(ellipse->top)) {
        ellipse->top -= offsetY;
      }
    } else if (type == NodeType::Polystar) {
      auto polystar = static_cast<Polystar*>(element);
      polystar->position.x -= offsetX;
      polystar->position.y -= offsetY;
      if (!std::isnan(polystar->left)) {
        polystar->left -= offsetX;
      }
      if (!std::isnan(polystar->top)) {
        polystar->top -= offsetY;
      }
    } else if (type == NodeType::Path) {
      auto path = static_cast<Path*>(element);
      if (!std::isnan(path->left)) {
        path->left -= offsetX;
      }
      if (!std::isnan(path->top)) {
        path->top -= offsetY;
      }
      if (path->data != nullptr && !path->data->isEmpty()) {
        auto it = offsetPathMap.find(path->data);
        if (it != offsetPathMap.end()) {
          path->data = it->second;
        } else {
          auto* original = path->data;
          auto newData = document->makeNode<PathData>(original->id);
          OffsetPathData(original, newData, offsetX, offsetY);
          offsetPathMap[original] = newData;
          path->data = newData;
        }
      }
    } else if (type == NodeType::Text) {
      auto text = static_cast<Text*>(element);
      text->position.x -= offsetX;
      text->position.y -= offsetY;
      if (!std::isnan(text->left)) {
        text->left -= offsetX;
      }
      if (!std::isnan(text->top)) {
        text->top -= offsetY;
      }
      for (auto* run : text->glyphRuns) {
        run->x -= offsetX;
        run->y -= offsetY;
      }
    } else if (type == NodeType::TextBox) {
      auto textBox = static_cast<TextBox*>(element);
      textBox->position.x -= offsetX;
      textBox->position.y -= offsetY;
      if (!std::isnan(textBox->left)) {
        textBox->left -= offsetX;
      }
      if (!std::isnan(textBox->top)) {
        textBox->top -= offsetY;
      }
    } else if (type == NodeType::Group) {
      auto group = static_cast<Group*>(element);
      group->position.x -= offsetX;
      group->position.y -= offsetY;
      if (!std::isnan(group->left)) {
        group->left -= offsetX;
      }
      if (!std::isnan(group->top)) {
        group->top -= offsetY;
      }
    }
  }
}

static void OffsetColorSourcesInContents(PAGXDocument* document,
                                         const std::vector<Element*>& contents, float offsetX,
                                         float offsetY) {
  std::unordered_map<const ColorSource*, ColorSource*> offsetColorMap = {};
  for (auto* element : contents) {
    auto type = element->nodeType();
    if (type == NodeType::Fill) {
      auto fill = static_cast<Fill*>(element);
      fill->color = OffsetColorSource(document, fill->color, offsetX, offsetY, offsetColorMap);
    } else if (type == NodeType::Stroke) {
      auto stroke = static_cast<Stroke*>(element);
      stroke->color = OffsetColorSource(document, stroke->color, offsetX, offsetY, offsetColorMap);
    }
  }
}

static void LocalizeLayerCoordinates(PAGXDocument* document, Layer* layer, int& count,
                                     bool parentHasContainerLayout) {
  // Skip localization when the parent uses container layout and this Layer participates —
  // the layout engine will overwrite x/y, so modifying them here would be lost.
  bool skipDueToParentLayout = parentHasContainerLayout && layer->includeInLayout;
  if (!skipDueToParentLayout && !ShouldSkipLocalization(layer)) {
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    ComputeLocalizationOffset(layer->contents, offsetX, offsetY);

    if (std::abs(offsetX) >= 0.001f || std::abs(offsetY) >= 0.001f) {
      layer->x += offsetX;
      layer->y += offsetY;
      ApplyLocalizationToElements(document, layer->contents, offsetX, offsetY);
      OffsetColorSourcesInContents(document, layer->contents, offsetX, offsetY);
      count++;
    }
  }
  bool thisHasContainerLayout = layer->layout != LayoutMode::None;
  for (auto* child : layer->children) {
    LocalizeLayerCoordinates(document, child, count, thisHasContainerLayout);
  }
}

static int LocalizeCoordinates(PAGXDocument* document) {
  int count = 0;

  for (auto* layer : document->layers) {
    LocalizeLayerCoordinates(document, layer, count, false);
  }
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto comp = static_cast<Composition*>(node.get());
      for (auto* layer : comp->layers) {
        LocalizeLayerCoordinates(document, layer, count, false);
      }
    }
  }
  return count;
}

// --- Optimization #9: Extract compositions ---

static size_t HashElement(const Element* element) {
  size_t h = std::hash<int>{}(static_cast<int>(element->nodeType()));
  auto type = element->nodeType();
  if (type == NodeType::Rectangle) {
    auto rect = static_cast<const Rectangle*>(element);
    h ^= std::hash<float>{}(rect->position.x) * 29;
    h ^= std::hash<float>{}(rect->position.y) * 31;
    h ^= std::hash<float>{}(rect->size.width) * 37;
    h ^= std::hash<float>{}(rect->size.height) * 41;
    h ^= std::hash<float>{}(rect->roundness) * 43;
    h ^= std::hash<bool>{}(rect->reversed) * 47;
  } else if (type == NodeType::Ellipse) {
    auto ellipse = static_cast<const Ellipse*>(element);
    h ^= std::hash<float>{}(ellipse->position.x) * 29;
    h ^= std::hash<float>{}(ellipse->position.y) * 31;
    h ^= std::hash<float>{}(ellipse->size.width) * 37;
    h ^= std::hash<float>{}(ellipse->size.height) * 41;
    h ^= std::hash<bool>{}(ellipse->reversed) * 43;
  } else if (type == NodeType::Polystar) {
    auto polystar = static_cast<const Polystar*>(element);
    h ^= std::hash<float>{}(polystar->position.x) * 29;
    h ^= std::hash<float>{}(polystar->position.y) * 31;
    h ^= std::hash<int>{}(static_cast<int>(polystar->type)) * 37;
    h ^= std::hash<float>{}(polystar->pointCount) * 41;
    h ^= std::hash<float>{}(polystar->outerRadius) * 43;
    h ^= std::hash<float>{}(polystar->innerRadius) * 47;
    h ^= std::hash<float>{}(polystar->rotation) * 53;
    h ^= std::hash<float>{}(polystar->outerRoundness) * 59;
    h ^= std::hash<float>{}(polystar->innerRoundness) * 61;
    h ^= std::hash<bool>{}(polystar->reversed) * 67;
  } else if (type == NodeType::Path) {
    auto path = static_cast<const Path*>(element);
    if (path->data != nullptr) {
      h ^= std::hash<size_t>{}(path->data->verbs().size()) * 31;
      h ^= std::hash<size_t>{}(path->data->points().size()) * 37;
    }
    h ^= std::hash<bool>{}(path->reversed) * 43;
  } else if (type == NodeType::Fill) {
    auto fill = static_cast<const Fill*>(element);
    h ^= std::hash<float>{}(fill->alpha) * 31;
    h ^= std::hash<int>{}(static_cast<int>(fill->blendMode)) * 37;
    h ^= std::hash<int>{}(static_cast<int>(fill->fillRule)) * 41;
    if (fill->color != nullptr) {
      h ^= std::hash<int>{}(static_cast<int>(fill->color->nodeType())) * 43;
    }
  } else if (type == NodeType::Stroke) {
    auto stroke = static_cast<const Stroke*>(element);
    h ^= std::hash<float>{}(stroke->width) * 31;
    h ^= std::hash<float>{}(stroke->alpha) * 37;
    h ^= std::hash<int>{}(static_cast<int>(stroke->cap)) * 41;
    h ^= std::hash<int>{}(static_cast<int>(stroke->join)) * 43;
  } else if (type == NodeType::Group) {
    auto group = static_cast<const Group*>(element);
    h ^= std::hash<float>{}(group->position.x) * 29;
    h ^= std::hash<float>{}(group->position.y) * 31;
    h ^= std::hash<float>{}(group->rotation) * 37;
    h ^= std::hash<float>{}(group->scale.x) * 41;
    h ^= std::hash<float>{}(group->scale.y) * 43;
    h ^= std::hash<float>{}(group->alpha) * 47;
    h ^= std::hash<size_t>{}(group->elements.size()) * 53;
  }
  return h;
}

static size_t HashLayerStructure(const Layer* layer) {
  size_t h = std::hash<size_t>{}(layer->contents.size());
  h ^= std::hash<size_t>{}(layer->children.size()) * 17;
  for (auto* element : layer->contents) {
    h = h * 131 + HashElement(element);
  }
  for (auto* child : layer->children) {
    h = h * 131 + HashLayerStructure(child);
  }
  return h;
}

static bool ElementsStructurallyEqual(const Element* a, const Element* b);

static bool PainterColorsEqual(const ColorSource* a, const ColorSource* b) {
  if (a == b) {
    return true;
  }
  if (a == nullptr || b == nullptr) {
    return false;
  }
  if (a->nodeType() != b->nodeType()) {
    return false;
  }
  // SolidColor values can be compared directly.
  if (a->nodeType() == NodeType::SolidColor) {
    return static_cast<const SolidColor*>(a)->color == static_cast<const SolidColor*>(b)->color;
  }
  // Gradients should have been deduplicated by DeduplicateColorSources earlier in the pipeline,
  // so identical gradients will share the same pointer. Different pointers mean different values.
  return false;
}

static bool ElementsStructurallyEqual(const Element* a, const Element* b) {
  if (a->nodeType() != b->nodeType()) {
    return false;
  }
  auto type = a->nodeType();
  if (type == NodeType::Rectangle) {
    auto ra = static_cast<const Rectangle*>(a);
    auto rb = static_cast<const Rectangle*>(b);
    return ra->position == rb->position && ra->size == rb->size && ra->roundness == rb->roundness &&
           ra->reversed == rb->reversed;
  }
  if (type == NodeType::Ellipse) {
    auto ea = static_cast<const Ellipse*>(a);
    auto eb = static_cast<const Ellipse*>(b);
    return ea->position == eb->position && ea->size == eb->size && ea->reversed == eb->reversed;
  }
  if (type == NodeType::Polystar) {
    auto pa = static_cast<const Polystar*>(a);
    auto pb = static_cast<const Polystar*>(b);
    return pa->position == pb->position && pa->type == pb->type &&
           pa->pointCount == pb->pointCount && pa->outerRadius == pb->outerRadius &&
           pa->innerRadius == pb->innerRadius && pa->rotation == pb->rotation &&
           pa->outerRoundness == pb->outerRoundness && pa->innerRoundness == pb->innerRoundness &&
           pa->reversed == pb->reversed;
  }
  if (type == NodeType::Path) {
    auto pathA = static_cast<const Path*>(a);
    auto pathB = static_cast<const Path*>(b);
    if (pathA->reversed != pathB->reversed) {
      return false;
    }
    return PathDataEqual(pathA->data, pathB->data);
  }
  if (type == NodeType::Fill) {
    auto fa = static_cast<const Fill*>(a);
    auto fb = static_cast<const Fill*>(b);
    return fa->alpha == fb->alpha && fa->blendMode == fb->blendMode &&
           fa->fillRule == fb->fillRule && fa->placement == fb->placement &&
           PainterColorsEqual(fa->color, fb->color);
  }
  if (type == NodeType::Stroke) {
    auto sa = static_cast<const Stroke*>(a);
    auto sb = static_cast<const Stroke*>(b);
    return sa->width == sb->width && sa->alpha == sb->alpha && sa->blendMode == sb->blendMode &&
           sa->cap == sb->cap && sa->join == sb->join && sa->miterLimit == sb->miterLimit &&
           sa->dashes == sb->dashes && sa->dashOffset == sb->dashOffset &&
           sa->dashAdaptive == sb->dashAdaptive && sa->align == sb->align &&
           sa->placement == sb->placement && PainterColorsEqual(sa->color, sb->color);
  }
  if (type == NodeType::Group) {
    auto ga = static_cast<const Group*>(a);
    auto gb = static_cast<const Group*>(b);
    if (ga->position != gb->position || ga->anchor != gb->anchor || ga->rotation != gb->rotation ||
        ga->scale != gb->scale || ga->skew != gb->skew || ga->skewAxis != gb->skewAxis ||
        ga->alpha != gb->alpha) {
      return false;
    }
    if (ga->elements.size() != gb->elements.size()) {
      return false;
    }
    for (size_t i = 0; i < ga->elements.size(); i++) {
      if (!ElementsStructurallyEqual(ga->elements[i], gb->elements[i])) {
        return false;
      }
    }
    return true;
  }
  if (type == NodeType::Text) {
    auto ta = static_cast<const Text*>(a);
    auto tb = static_cast<const Text*>(b);
    if (ta->text != tb->text || ta->fontFamily != tb->fontFamily ||
        ta->fontStyle != tb->fontStyle || ta->fontSize != tb->fontSize ||
        ta->letterSpacing != tb->letterSpacing || ta->fauxBold != tb->fauxBold ||
        ta->fauxItalic != tb->fauxItalic || ta->textAnchor != tb->textAnchor ||
        ta->glyphRuns.size() != tb->glyphRuns.size()) {
      return false;
    }
    for (size_t i = 0; i < ta->glyphRuns.size(); i++) {
      auto ra = ta->glyphRuns[i];
      auto rb = tb->glyphRuns[i];
      if (ra->font != rb->font || ra->fontSize != rb->fontSize || ra->glyphs != rb->glyphs ||
          ra->x != rb->x || ra->y != rb->y || ra->xOffsets != rb->xOffsets ||
          ra->positions != rb->positions || ra->anchors != rb->anchors ||
          ra->scales != rb->scales || ra->rotations != rb->rotations || ra->skews != rb->skews) {
        return false;
      }
    }
    return true;
  }
  if (type == NodeType::TextBox) {
    auto tba = static_cast<const TextBox*>(a);
    auto tbb = static_cast<const TextBox*>(b);
    return tba->position == tbb->position &&
           (tba->width == tbb->width || (std::isnan(tba->width) && std::isnan(tbb->width))) &&
           (tba->height == tbb->height || (std::isnan(tba->height) && std::isnan(tbb->height))) &&
           tba->textAlign == tbb->textAlign && tba->paragraphAlign == tbb->paragraphAlign &&
           tba->writingMode == tbb->writingMode && tba->lineHeight == tbb->lineHeight &&
           tba->wordWrap == tbb->wordWrap && tba->overflow == tbb->overflow;
  }
  // For other element types (TrimPath, RoundCorner, MergePath, Repeater, TextModifier, TextPath),
  // default to not equal to be conservative.
  return false;
}

static bool LayersStructurallyEqual(const Layer* a, const Layer* b) {
  if (a->contents.size() != b->contents.size()) {
    return false;
  }
  if (a->children.size() != b->children.size()) {
    return false;
  }
  if (a->composition != nullptr || b->composition != nullptr) {
    return false;
  }
  if (a->styles.size() != b->styles.size() || a->filters.size() != b->filters.size()) {
    return false;
  }
  if (a->alpha != b->alpha || a->blendMode != b->blendMode || a->visible != b->visible ||
      a->maskType != b->maskType || a->antiAlias != b->antiAlias ||
      a->groupOpacity != b->groupOpacity) {
    return false;
  }
  if (!a->matrix.isIdentity() || !b->matrix.isIdentity()) {
    return false;
  }
  for (size_t i = 0; i < a->contents.size(); i++) {
    if (!ElementsStructurallyEqual(a->contents[i], b->contents[i])) {
      return false;
    }
  }
  for (size_t i = 0; i < a->children.size(); i++) {
    if (!LayersStructurallyEqual(a->children[i], b->children[i])) {
      return false;
    }
  }
  return true;
}

static void ComputeLayerContentsBounds(const Layer* layer, float& boundsMinX, float& boundsMinY,
                                       float& boundsWidth, float& boundsHeight) {
  float minX = 0.0f;
  float minY = 0.0f;
  float maxX = 0.0f;
  float maxY = 0.0f;
  if (ComputeShapeBounds(layer->contents, minX, minY, maxX, maxY)) {
    boundsMinX = minX;
    boundsMinY = minY;
    boundsWidth = maxX - minX;
    boundsHeight = maxY - minY;
  } else {
    boundsMinX = 0.0f;
    boundsMinY = 0.0f;
    boundsWidth = 0.0f;
    boundsHeight = 0.0f;
  }
}

static std::string GenerateUniqueId(PAGXDocument* document, const std::string& prefix) {
  int counter = 1;
  std::string id = {};
  do {
    id = prefix + std::to_string(counter++);
  } while (document->findNode(id) != nullptr);
  return id;
}

static void CollectCandidateLayers(Layer* layer,
                                   std::unordered_map<size_t, std::vector<Layer*>>& hashGroups) {
  if (!layer->contents.empty() && layer->composition == nullptr && layer->matrix.isIdentity() &&
      layer->children.empty() && layer->styles.empty() && layer->filters.empty() &&
      // Exclude layers with flex sizing — their dimensions are dynamically assigned by the parent
      // container layout, so a fixed-size Composition would not match the actual rendered size.
      layer->flex == 0.0f &&
      // Exclude layers with constraint attributes that depend on parent dimensions — opposite-edge
      // constraints (right, bottom) and alignment constraints (centerX, centerY) make the layer's
      // position/size parent-dependent, so a fixed-size Composition would not match across instances.
      // Single-edge constraints (left, top) are equivalent to x/y positioning and are safe to extract.
      std::isnan(layer->right) && std::isnan(layer->bottom) && std::isnan(layer->centerX) &&
      std::isnan(layer->centerY)) {
    auto hash = HashLayerStructure(layer);
    hashGroups[hash].push_back(layer);
  }
  for (auto* child : layer->children) {
    CollectCandidateLayers(child, hashGroups);
  }
}

static int ExtractCompositions(PAGXDocument* document) {
  // Step 1: Collect all candidate layers (with contents, no composition reference, identity matrix).
  std::unordered_map<size_t, std::vector<Layer*>> hashGroups = {};

  for (auto* layer : document->layers) {
    CollectCandidateLayers(layer, hashGroups);
  }

  // Step 2: Find groups with 2+ structurally identical layers.
  struct ExtractionCandidate {
    std::vector<Layer*> layers = {};
    float minX = 0.0f;
    float minY = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
  };
  std::vector<ExtractionCandidate> candidates = {};

  for (auto& entry : hashGroups) {
    if (entry.second.size() < 2) {
      continue;
    }
    // Verify structural equality (handle hash collisions).
    std::vector<std::vector<Layer*>> equalGroups = {};
    for (auto* layer : entry.second) {
      bool found = false;
      for (auto& group : equalGroups) {
        if (LayersStructurallyEqual(layer, group[0])) {
          group.push_back(layer);
          found = true;
          break;
        }
      }
      if (!found) {
        equalGroups.push_back({layer});
      }
    }
    for (auto& group : equalGroups) {
      if (group.size() >= 2) {
        ExtractionCandidate candidate = {};
        candidate.layers = group;
        ComputeLayerContentsBounds(group[0], candidate.minX, candidate.minY, candidate.width,
                                   candidate.height);
        if (candidate.width > 0.0f && candidate.height > 0.0f) {
          candidates.push_back(candidate);
        }
      }
    }
  }

  // Step 3: Create Composition for each candidate group.
  int count = 0;
  for (auto& candidate : candidates) {
    auto* sourceLayer = candidate.layers[0];

    // Create the Composition with a single inner Layer that holds the contents.
    auto compId = GenerateUniqueId(document, "comp");
    auto comp = document->makeNode<Composition>(compId);
    comp->width = candidate.width;
    comp->height = candidate.height;

    auto innerLayer = document->makeNode<Layer>(GenerateUniqueId(document, "compLayer"));
    // Move contents from the source layer to the inner layer.
    innerLayer->contents = sourceLayer->contents;
    // Localize contents coordinates to Composition space (origin at top-left of bounds).
    if (std::abs(candidate.minX) >= 0.001f || std::abs(candidate.minY) >= 0.001f) {
      ApplyLocalizationToElements(document, innerLayer->contents, candidate.minX, candidate.minY);
      OffsetColorSourcesInContents(document, innerLayer->contents, candidate.minX, candidate.minY);
    }
    comp->layers.push_back(innerLayer);

    // Replace each layer's contents with a composition reference. Adjust each layer's x/y
    // (and left/top if set) to compensate for the coordinate shift applied inside the Composition.
    for (auto* layer : candidate.layers) {
      layer->contents.clear();
      layer->composition = comp;
      layer->x += candidate.minX;
      layer->y += candidate.minY;
      if (!std::isnan(layer->left)) {
        layer->left += candidate.minX;
      }
      if (!std::isnan(layer->top)) {
        layer->top += candidate.minY;
      }
    }
    count += static_cast<int>(candidate.layers.size());
  }

  return count;
}

// --- Main optimize pipeline ---

static OptimizeReport OptimizeDocument(PAGXDocument* document) {
  OptimizeReport report = {};
  report.emptyElements = RemoveEmptyNodes(document);
  report.deduplicatedPathData = DeduplicatePathData(document);
  report.deduplicatedGradients = DeduplicateColorSources(document);
  report.mergedGroups = MergeAdjacentGroups(document);
  ReplacePathsWithPrimitives(document, report.pathToRectangle, report.pathToEllipse);
  report.fullCanvasClips = RemoveFullCanvasClipMasks(document, document->layers);
  report.offCanvasLayers = RemoveOffCanvasLayers(document);
  report.coordinatesLocalized = LocalizeCoordinates(document);
  report.compositionsExtracted = ExtractCompositions(document);
  report.unreferencedResources = RemoveUnreferencedResources(document);
  return report;
}

int RunOptimize(int argc, char* argv[]) {
  std::string inputPath = {};
  std::string outputPath = {};
  bool dryRun = false;

  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
      PrintOptimizeUsage();
      return 0;
    }
    if ((std::strcmp(argv[i], "-o") == 0 || std::strcmp(argv[i], "--output") == 0) &&
        i + 1 < argc) {
      outputPath = argv[++i];
      continue;
    }
    if (std::strcmp(argv[i], "--dry-run") == 0) {
      dryRun = true;
      continue;
    }
    if (argv[i][0] == '-') {
      std::cerr << "pagx optimize: unknown option '" << argv[i] << "'\n";
      return 1;
    }
    if (inputPath.empty()) {
      inputPath = argv[i];
    } else {
      std::cerr << "pagx optimize: unexpected argument '" << argv[i] << "'\n";
      return 1;
    }
  }

  if (inputPath.empty()) {
    std::cerr << "pagx optimize: missing input file\n";
    PrintOptimizeUsage();
    return 1;
  }

  if (outputPath.empty()) {
    outputPath = inputPath;
  }

  // Validate input against XSD schema before optimizing.
  auto validationErrors = ValidateFile(inputPath);
  if (!validationErrors.empty()) {
    std::cerr << "pagx optimize: validation failed for '" << inputPath << "'\n";
    for (const auto& error : validationErrors) {
      if (error.line > 0) {
        std::cerr << inputPath << ":" << error.line << ": " << error.message << "\n";
      } else {
        std::cerr << inputPath << ": " << error.message << "\n";
      }
    }
    return 1;
  }

  auto document = PAGXImporter::FromFile(inputPath);
  if (document == nullptr) {
    std::cerr << "pagx optimize: failed to load '" << inputPath << "'\n";
    return 1;
  }

  auto report = OptimizeDocument(document.get());
  report.print();
  auto hints = CollectLintHints(document.get());
  PrintLintHints(hints);

  if (dryRun) {
    return 0;
  }

  auto xml = PAGXExporter::ToXML(*document);

  std::ofstream out(outputPath);
  if (!out.is_open()) {
    std::cerr << "pagx optimize: failed to write '" << outputPath << "'\n";
    return 1;
  }
  out << xml;
  out.close();
  if (out.fail()) {
    std::cerr << "pagx optimize: error writing to '" << outputPath << "'\n";
    return 1;
  }

  return 0;
}

}  // namespace pagx::cli
