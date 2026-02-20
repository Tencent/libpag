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

#include "CommandOptimize.h"
#include "CommandValidator.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/GlyphRun.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
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
  int unreferencedResources = 0;
  int pathToRectangle = 0;
  int pathToEllipse = 0;
  int fullCanvasClips = 0;
  int offCanvasLayers = 0;
  int coordinatesLocalized = 0;
  int compositionsExtracted = 0;

  int total() const {
    return emptyElements + deduplicatedPathData + deduplicatedGradients + unreferencedResources +
           pathToRectangle + pathToEllipse + fullCanvasClips + offCanvasLayers +
           coordinatesLocalized + compositionsExtracted;
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
            << "  1. Remove empty elements (empty Layer/Group, zero-width Stroke)\n"
            << "  2. Deduplicate PathData resources\n"
            << "  3. Deduplicate gradient resources\n"
            << "  4. Remove unreferenced resources\n"
            << "  5. Replace Path with Rectangle/Ellipse\n"
            << "  6. Remove full-canvas clip masks\n"
            << "  7. Remove off-canvas invisible layers\n"
            << "  8. Localize layer coordinates\n"
            << "  9. Extract duplicate layers to compositions\n"
            << "\n"
            << "Options:\n"
            << "  -o, --output <path>   Output file path (default: overwrite input)\n"
            << "  --dry-run             Only print report, do not write output\n"
            << "  -h, --help            Show this help message\n";
}

// --- Optimization #1: Remove empty elements ---

static bool IsEmptyLayer(const Layer* layer) {
  return layer->contents.empty() && layer->children.empty() && layer->composition == nullptr &&
         layer->styles.empty() && layer->filters.empty();
}

static int RemoveEmptyLayers(std::vector<Layer*>& layers) {
  auto originalSize = static_cast<int>(layers.size());
  layers.erase(std::remove_if(layers.begin(), layers.end(), IsEmptyLayer), layers.end());
  return originalSize - static_cast<int>(layers.size());
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
  count += RemoveEmptyLayers(document->layers);
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Layer) {
      auto layer = static_cast<Layer*>(node.get());
      count += RemoveEmptyLayers(layer->children);
      count += RemoveEmptyElements(layer->contents);
    } else if (node->nodeType() == NodeType::Group) {
      auto group = static_cast<Group*>(node.get());
      count += RemoveEmptyElements(group->elements);
    } else if (node->nodeType() == NodeType::Composition) {
      auto composition = static_cast<Composition*>(node.get());
      count += RemoveEmptyLayers(composition->layers);
    }
  }
  return count;
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
    } else if (type == NodeType::Group) {
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
  auto originalSize = static_cast<int>(nodes.size());
  int writeIndex = 0;
  for (int i = 0; i < static_cast<int>(nodes.size()); i++) {
    if (referenced.find(nodes[i].get()) != referenced.end()) {
      if (writeIndex != i) {
        nodes[writeIndex] = std::move(nodes[i]);
      }
      writeIndex++;
    }
  }
  nodes.resize(writeIndex);
  return originalSize - writeIndex;
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
    } else if (node->nodeType() == NodeType::Group) {
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
      if (tgfxPath.isRRect(&rrect)) {
        PrimitiveReplacement rep = {};
        rep.elements = elements;
        rep.index = i;
        rep.bounds = rrect.rect;
        rep.radiusX = rrect.radii.x;
        rep.reversed = path->reversed;
        rep.type = rrect.isOval() ? PrimitiveType::Oval : PrimitiveType::RoundRect;
        replacements.push_back(rep);
      } else if (tgfxPath.isOval(&rect)) {
        PrimitiveReplacement rep = {};
        rep.elements = elements;
        rep.index = i;
        rep.bounds = rect;
        rep.reversed = path->reversed;
        rep.type = PrimitiveType::Oval;
        replacements.push_back(rep);
      } else if (tgfxPath.isRect(&rect)) {
        PrimitiveReplacement rep = {};
        rep.elements = elements;
        rep.index = i;
        rep.bounds = rect;
        rep.reversed = path->reversed;
        rep.type = PrimitiveType::Rect;
        replacements.push_back(rep);
      }
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
      ellipse->center = {centerX, centerY};
      ellipse->size = {width, height};
      ellipse->reversed = rep.reversed;
      (*rep.elements)[rep.index] = ellipse;
      ellipseCount++;
    } else {
      auto rectangle = document->makeNode<Rectangle>();
      rectangle->center = {centerX, centerY};
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
  if (maskLayer->contents.size() != 1) {
    return false;
  }
  auto element = maskLayer->contents[0];
  if (element->nodeType() != NodeType::Rectangle) {
    return false;
  }
  auto rect = static_cast<const Rectangle*>(element);
  auto left = rect->center.x - rect->size.width * 0.5f;
  auto top = rect->center.y - rect->size.height * 0.5f;
  return left <= 0 && top <= 0 && rect->size.width >= canvasWidth &&
         rect->size.height >= canvasHeight;
}

static int RemoveFullCanvasClipMasks(PAGXDocument* document,
                                     const std::vector<Layer*>& layers) {
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

static int RemoveOffCanvasLayers(PAGXDocument* document) {
  auto rootLayer = LayerBuilder::Build(document);
  if (rootLayer == nullptr) {
    return 0;
  }
  auto canvasRect = tgfx::Rect::MakeWH(document->width, document->height);
  auto& rootChildren = rootLayer->children();
  if (rootChildren.size() != document->layers.size()) {
    return 0;
  }
  std::unordered_set<const Layer*> maskedLayers = {};
  CollectMaskReferences(document->layers, maskedLayers);
  std::unordered_set<size_t> indicesToRemove = {};
  for (size_t i = 0; i < document->layers.size(); i++) {
    auto* pagxLayer = document->layers[i];
    if (!pagxLayer->visible) {
      continue;
    }
    if (maskedLayers.find(pagxLayer) != maskedLayers.end()) {
      continue;
    }
    if (!pagxLayer->styles.empty() || !pagxLayer->filters.empty()) {
      continue;
    }
    auto tgfxChild = rootChildren[i];
    auto bounds = tgfxChild->getBounds(nullptr, true);
    if (!tgfx::Rect::Intersects(bounds, canvasRect)) {
      indicesToRemove.insert(i);
    }
  }
  if (indicesToRemove.empty()) {
    return 0;
  }
  auto& layers = document->layers;
  int writeIndex = 0;
  for (int i = 0; i < static_cast<int>(layers.size()); i++) {
    if (indicesToRemove.find(static_cast<size_t>(i)) == indicesToRemove.end()) {
      layers[writeIndex++] = layers[i];
    }
  }
  layers.resize(writeIndex);
  return static_cast<int>(indicesToRemove.size());
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
  return false;
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

  // No TextBox - compute bounding box center of geometry elements.
  float minX = FLT_MAX;
  float minY = FLT_MAX;
  float maxX = -FLT_MAX;
  float maxY = -FLT_MAX;
  bool hasGeometry = false;

  for (auto* element : contents) {
    auto type = element->nodeType();
    if (type == NodeType::Rectangle) {
      auto rect = static_cast<const Rectangle*>(element);
      float halfW = rect->size.width * 0.5f;
      float halfH = rect->size.height * 0.5f;
      minX = std::min(minX, rect->center.x - halfW);
      minY = std::min(minY, rect->center.y - halfH);
      maxX = std::max(maxX, rect->center.x + halfW);
      maxY = std::max(maxY, rect->center.y + halfH);
      hasGeometry = true;
    } else if (type == NodeType::Ellipse) {
      auto ellipse = static_cast<const Ellipse*>(element);
      float halfW = ellipse->size.width * 0.5f;
      float halfH = ellipse->size.height * 0.5f;
      minX = std::min(minX, ellipse->center.x - halfW);
      minY = std::min(minY, ellipse->center.y - halfH);
      maxX = std::max(maxX, ellipse->center.x + halfW);
      maxY = std::max(maxY, ellipse->center.y + halfH);
      hasGeometry = true;
    } else if (type == NodeType::Polystar) {
      auto polystar = static_cast<const Polystar*>(element);
      float r = polystar->outerRadius;
      minX = std::min(minX, polystar->center.x - r);
      minY = std::min(minY, polystar->center.y - r);
      maxX = std::max(maxX, polystar->center.x + r);
      maxY = std::max(maxY, polystar->center.y + r);
      hasGeometry = true;
    } else if (type == NodeType::Text) {
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
    offsetX = (minX + maxX) * 0.5f;
    offsetY = (minY + maxY) * 0.5f;
  }
}

static void ApplyLocalizationToElements(const std::vector<Element*>& contents, float offsetX,
                                        float offsetY) {
  for (auto* element : contents) {
    auto type = element->nodeType();
    if (type == NodeType::Rectangle) {
      auto rect = static_cast<Rectangle*>(element);
      rect->center.x -= offsetX;
      rect->center.y -= offsetY;
    } else if (type == NodeType::Ellipse) {
      auto ellipse = static_cast<Ellipse*>(element);
      ellipse->center.x -= offsetX;
      ellipse->center.y -= offsetY;
    } else if (type == NodeType::Polystar) {
      auto polystar = static_cast<Polystar*>(element);
      polystar->center.x -= offsetX;
      polystar->center.y -= offsetY;
    } else if (type == NodeType::Text) {
      auto text = static_cast<Text*>(element);
      text->position.x -= offsetX;
      text->position.y -= offsetY;
      for (auto* run : text->glyphRuns) {
        run->x -= offsetX;
        run->y -= offsetY;
      }
    } else if (type == NodeType::TextBox) {
      auto textBox = static_cast<TextBox*>(element);
      textBox->position.x -= offsetX;
      textBox->position.y -= offsetY;
    } else if (type == NodeType::Group) {
      auto group = static_cast<Group*>(element);
      group->position.x -= offsetX;
      group->position.y -= offsetY;
    }
  }
}

static void LocalizeLayerCoordinates(Layer* layer, int& count) {
  if (!ShouldSkipLocalization(layer)) {
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    ComputeLocalizationOffset(layer->contents, offsetX, offsetY);

    if (std::abs(offsetX) >= 0.001f || std::abs(offsetY) >= 0.001f) {
      layer->x += offsetX;
      layer->y += offsetY;
      ApplyLocalizationToElements(layer->contents, offsetX, offsetY);
      count++;
    }
  }
  for (auto* child : layer->children) {
    LocalizeLayerCoordinates(child, count);
  }
}

static int LocalizeCoordinates(PAGXDocument* document) {
  int count = 0;

  for (auto* layer : document->layers) {
    LocalizeLayerCoordinates(layer, count);
  }
  for (auto& node : document->nodes) {
    if (node->nodeType() == NodeType::Composition) {
      auto comp = static_cast<Composition*>(node.get());
      for (auto* layer : comp->layers) {
        LocalizeLayerCoordinates(layer, count);
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
    h ^= std::hash<float>{}(rect->size.width) * 31;
    h ^= std::hash<float>{}(rect->size.height) * 37;
    h ^= std::hash<float>{}(rect->roundness) * 41;
    h ^= std::hash<bool>{}(rect->reversed) * 43;
  } else if (type == NodeType::Ellipse) {
    auto ellipse = static_cast<const Ellipse*>(element);
    h ^= std::hash<float>{}(ellipse->size.width) * 31;
    h ^= std::hash<float>{}(ellipse->size.height) * 37;
    h ^= std::hash<bool>{}(ellipse->reversed) * 43;
  } else if (type == NodeType::Polystar) {
    auto polystar = static_cast<const Polystar*>(element);
    h ^= std::hash<int>{}(static_cast<int>(polystar->type)) * 31;
    h ^= std::hash<float>{}(polystar->pointCount) * 37;
    h ^= std::hash<float>{}(polystar->outerRadius) * 41;
    h ^= std::hash<float>{}(polystar->innerRadius) * 43;
    h ^= std::hash<float>{}(polystar->rotation) * 47;
    h ^= std::hash<float>{}(polystar->outerRoundness) * 53;
    h ^= std::hash<float>{}(polystar->innerRoundness) * 59;
    h ^= std::hash<bool>{}(polystar->reversed) * 61;
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
    h ^= std::hash<float>{}(group->rotation) * 31;
    h ^= std::hash<float>{}(group->scale.x) * 37;
    h ^= std::hash<float>{}(group->scale.y) * 41;
    h ^= std::hash<float>{}(group->alpha) * 43;
    h ^= std::hash<size_t>{}(group->elements.size()) * 47;
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
    return ra->size == rb->size && ra->roundness == rb->roundness && ra->reversed == rb->reversed;
  }
  if (type == NodeType::Ellipse) {
    auto ea = static_cast<const Ellipse*>(a);
    auto eb = static_cast<const Ellipse*>(b);
    return ea->size == eb->size && ea->reversed == eb->reversed;
  }
  if (type == NodeType::Polystar) {
    auto pa = static_cast<const Polystar*>(a);
    auto pb = static_cast<const Polystar*>(b);
    return pa->type == pb->type && pa->pointCount == pb->pointCount &&
           pa->outerRadius == pb->outerRadius && pa->innerRadius == pb->innerRadius &&
           pa->rotation == pb->rotation && pa->outerRoundness == pb->outerRoundness &&
           pa->innerRoundness == pb->innerRoundness && pa->reversed == pb->reversed;
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
    if (ga->anchor != gb->anchor || ga->rotation != gb->rotation || ga->scale != gb->scale ||
        ga->skew != gb->skew || ga->skewAxis != gb->skewAxis || ga->alpha != gb->alpha) {
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
    return ta->text == tb->text && ta->fontFamily == tb->fontFamily &&
           ta->fontStyle == tb->fontStyle && ta->fontSize == tb->fontSize &&
           ta->letterSpacing == tb->letterSpacing && ta->fauxBold == tb->fauxBold &&
           ta->fauxItalic == tb->fauxItalic && ta->textAnchor == tb->textAnchor &&
           ta->glyphRuns.size() == tb->glyphRuns.size();
  }
  if (type == NodeType::TextBox) {
    auto tba = static_cast<const TextBox*>(a);
    auto tbb = static_cast<const TextBox*>(b);
    return tba->size == tbb->size && tba->textAlign == tbb->textAlign &&
           tba->paragraphAlign == tbb->paragraphAlign && tba->writingMode == tbb->writingMode &&
           tba->lineHeight == tbb->lineHeight && tba->wordWrap == tbb->wordWrap &&
           tba->overflow == tbb->overflow;
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
  if (a->alpha != b->alpha || a->blendMode != b->blendMode || a->visible != b->visible) {
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

static void ComputeLayerContentsBounds(const Layer* layer, float& boundsWidth,
                                       float& boundsHeight) {
  float minX = FLT_MAX;
  float minY = FLT_MAX;
  float maxX = -FLT_MAX;
  float maxY = -FLT_MAX;
  bool hasGeometry = false;

  for (auto* element : layer->contents) {
    auto type = element->nodeType();
    if (type == NodeType::Rectangle) {
      auto rect = static_cast<const Rectangle*>(element);
      float halfW = rect->size.width * 0.5f;
      float halfH = rect->size.height * 0.5f;
      minX = std::min(minX, rect->center.x - halfW);
      minY = std::min(minY, rect->center.y - halfH);
      maxX = std::max(maxX, rect->center.x + halfW);
      maxY = std::max(maxY, rect->center.y + halfH);
      hasGeometry = true;
    } else if (type == NodeType::Ellipse) {
      auto ellipse = static_cast<const Ellipse*>(element);
      float halfW = ellipse->size.width * 0.5f;
      float halfH = ellipse->size.height * 0.5f;
      minX = std::min(minX, ellipse->center.x - halfW);
      minY = std::min(minY, ellipse->center.y - halfH);
      maxX = std::max(maxX, ellipse->center.x + halfW);
      maxY = std::max(maxY, ellipse->center.y + halfH);
      hasGeometry = true;
    } else if (type == NodeType::Polystar) {
      auto polystar = static_cast<const Polystar*>(element);
      float r = polystar->outerRadius;
      minX = std::min(minX, polystar->center.x - r);
      minY = std::min(minY, polystar->center.y - r);
      maxX = std::max(maxX, polystar->center.x + r);
      maxY = std::max(maxY, polystar->center.y + r);
      hasGeometry = true;
    }
  }

  if (hasGeometry) {
    boundsWidth = maxX - minX;
    boundsHeight = maxY - minY;
  } else {
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
      layer->children.empty() && layer->styles.empty() && layer->filters.empty()) {
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
    float width = 0.0f;
    float height = 0.0f;
  };
  std::vector<ExtractionCandidate> candidates = {};

  for (auto& [hash, layers] : hashGroups) {
    if (layers.size() < 2) {
      continue;
    }
    // Verify structural equality (handle hash collisions).
    std::vector<std::vector<Layer*>> equalGroups = {};
    for (auto* layer : layers) {
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
        ComputeLayerContentsBounds(group[0], candidate.width, candidate.height);
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
    comp->layers.push_back(innerLayer);

    // Replace each layer's contents with a composition reference.
    for (auto* layer : candidate.layers) {
      if (layer != sourceLayer) {
        // Other layers lose their contents (they were structurally identical).
        layer->contents.clear();
      } else {
        // Source layer's contents were moved to innerLayer.
        layer->contents.clear();
      }
      layer->composition = comp;
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

  return 0;
}

}  // namespace pagx::cli
