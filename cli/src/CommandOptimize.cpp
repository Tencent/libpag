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
#include <algorithm>
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
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/PathData.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/Repeater.h"
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

  int total() const {
    return emptyElements + deduplicatedPathData + deduplicatedGradients + unreferencedResources +
           pathToRectangle + pathToEllipse + fullCanvasClips + offCanvasLayers;
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
  }
};

static void PrintOptimizeUsage() {
  std::cout << "Usage: pagx optimize [options] <file.pagx>\n"
            << "\n"
            << "Applies deterministic structural optimizations to a PAGX file.\n"
            << "\n"
            << "Optimizations:\n"
            << "  1. Remove empty elements (empty Layer/Group, zero-width Stroke)\n"
            << "  2. Deduplicate PathData resources\n"
            << "  3. Deduplicate gradient resources\n"
            << "  4. Remove unreferenced resources\n"
            << "  5. Replace Path with Rectangle/Ellipse\n"
            << "  6. Remove full-canvas clip masks\n"
            << "  7. Remove off-canvas invisible layers\n"
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
  layers.erase(
      std::remove_if(layers.begin(), layers.end(),
                     [](const Layer* layer) { return IsEmptyLayer(layer); }),
      layers.end());
  return originalSize - static_cast<int>(layers.size());
}

static int RemoveEmptyElements(std::vector<Element*>& elements) {
  auto originalSize = static_cast<int>(elements.size());
  elements.erase(
      std::remove_if(elements.begin(), elements.end(),
                     [](const Element* element) {
                       if (element->nodeType() == NodeType::Stroke) {
                         auto stroke = static_cast<const Stroke*>(element);
                         return stroke->width == 0;
                       }
                       if (element->nodeType() == NodeType::Group) {
                         auto group = static_cast<const Group*>(element);
                         return group->elements.empty();
                       }
                       return false;
                     }),
      elements.end());
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

static void CollectReferencedNodesFromColorSource(const ColorSource* source,
                                                  std::unordered_set<const Node*>& referenced) {
  if (source == nullptr) {
    return;
  }
  referenced.insert(source);
  auto type = source->nodeType();
  if (type == NodeType::LinearGradient) {
    auto gradient = static_cast<const LinearGradient*>(source);
    for (auto* stop : gradient->colorStops) {
      referenced.insert(stop);
    }
  } else if (type == NodeType::RadialGradient) {
    auto gradient = static_cast<const RadialGradient*>(source);
    for (auto* stop : gradient->colorStops) {
      referenced.insert(stop);
    }
  } else if (type == NodeType::ConicGradient) {
    auto gradient = static_cast<const ConicGradient*>(source);
    for (auto* stop : gradient->colorStops) {
      referenced.insert(stop);
    }
  } else if (type == NodeType::DiamondGradient) {
    auto gradient = static_cast<const DiamondGradient*>(source);
    for (auto* stop : gradient->colorStops) {
      referenced.insert(stop);
    }
  } else if (type == NodeType::ImagePattern) {
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
  nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
                              [&referenced](const std::unique_ptr<Node>& node) {
                                return referenced.find(node.get()) == referenced.end();
                              }),
              nodes.end());
  return originalSize - static_cast<int>(nodes.size());
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

// --- Main optimize pipeline ---

static OptimizeReport OptimizeDocument(PAGXDocument* document) {
  OptimizeReport report = {};
  report.emptyElements = RemoveEmptyNodes(document);
  report.deduplicatedPathData = DeduplicatePathData(document);
  report.deduplicatedGradients = DeduplicateColorSources(document);
  ReplacePathsWithPrimitives(document, report.pathToRectangle, report.pathToEllipse);
  report.fullCanvasClips = RemoveFullCanvasClipMasks(document, document->layers);
  report.offCanvasLayers = RemoveOffCanvasLayers(document);
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
